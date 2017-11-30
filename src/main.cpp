#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "driver/gpio.h"

#include "DoubleBuffer.hpp"
#include "Sensor.hpp"
#include "SDWriter.hpp"
#include "WifiModule.hpp"
#include "Mpu9250Implementation.hpp"
#include "Bmp280Implementation.hpp"

#include "freertos/event_groups.h"

#include "DoubleBuffer.hpp"
#include "SDWriter.hpp"

#define BIT_0  ( 1 << 0 )
#define BIT_4  ( 1 << 4 )

// -------Flags for Tasks----------------------------
// Flag for SensorTask to act upon
const EventBits_t SensorMeasurementFlag = ( 1 << 0 );

// Flag for SdWriterTask to act upon
const EventBits_t SensorBufferSdReady   = ( 1 << 4 );

// Flags for WifiTask to act upon
const EventBits_t WifiActivateFlag      = (1 << 1);
const EventBits_t WifiReadyFlag         = (1 << 2);

// Flags for system alive (watchdog timer)
const EventBits_t SystemAlive          = (1 << 3);
//-------------------------------------------------

EventGroupHandle_t evg;

// Define led pin
#define BLINK_GPIO 13

void blink_task(void *pvParameter)
{
    gpio_pad_select_gpio((gpio_num_t)BLINK_GPIO);
    // Set the GPIO as a push/pull output
    gpio_set_direction((gpio_num_t)BLINK_GPIO, (gpio_mode_t)GPIO_MODE_OUTPUT);
    while(1) {
        // Blink off (output low)
        gpio_set_level((gpio_num_t)BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // Blink on (output high)
        gpio_set_level((gpio_num_t)BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void wifi_task(void *pvParameter) {

	char* ip = "192.168.43.140";
	char* subnet = "192.168.43.1";
	char* mask = "255.255.255.0";
	char* ssid = "Allyouare";
	char* pass = "Meulen-2017";
	WifiModule wificonnection;
	wificonnection.ClientSetStaticIP(ip, subnet, mask);
	wificonnection.ClientConfig(ssid, pass);
	wificonnection.ClientConnect(10000);

	while(1){
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

static void i2c_example_master_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_25;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_26;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

void sample_task(void *pvParameter) {
	i2c_example_master_init();

	Sensor *TestMPU = new Mpu9250Implementation();
	Sensor *TestBMP = new Bmp280Implementation();
	data SensorData;
	short MPUData[9];
	int BMPData[2];

	while(1) {
		memcpy(MPUData, TestMPU->SensorRead(), sizeof(unsigned short) * 9);
		memcpy(BMPData, TestBMP->SensorRead(), sizeof(int) * 2);

		SensorData.accelX = (float)MPUData[0];
		SensorData.accelY = (float)MPUData[1];
		SensorData.accelZ = (float)MPUData[2];
		SensorData.gyroX = (float)MPUData[3];
		SensorData.gyroY = (float)MPUData[4];
		SensorData.gyroZ = (float)MPUData[5];
		SensorData.magnetoX = (float)MPUData[6];
		SensorData.magnetoY = (float)MPUData[7];
		SensorData.magnetoZ = (float)MPUData[8];
		SensorData.temp = (float)BMPData[0];
		SensorData.pressure = (float)BMPData[1];

		ESP_LOGI("I2C TASK", "Value: \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f, \t %.0f \t",
				SensorData.accelX/100,
				SensorData.accelY/100,
				SensorData.accelZ/100,
				SensorData.gyroX/100,
				SensorData.gyroY/100,
				SensorData.gyroZ/100,
				SensorData.magnetoX/100,
				SensorData.magnetoY/100,
				SensorData.magnetoZ/100,
				SensorData.temp,
				SensorData.pressure);


		vTaskDelay(6 / portTICK_PERIOD_MS);
	}
}

void writer_task(void *pvParameter) {
   
	while(1) {
      EventBits_t uxBits;
      uxBits = xEventGroupWaitBits(evg, SensorBufferSdReady /*| SystemAlive*/, pdTRUE, pdFALSE, portMAX_DELAY );

      ESP_LOGI("MAIN", "Stopped waiting...");
      if(uxBits & SensorBufferSdReady){
         // Todo handle writing of buffer to sd card...
         ESP_LOGI("MAIN", "SensorBufferSdReady");
         db.writeToSd();

      }
      /*else if(uxBits & SystemAlive) {
         // Do nothing
         vTaskDelay(100 / portTICK_PERIOD_MS);
         ESP_LOGI("MAIN", "SystemAlive set");
      }*/
      else {
         ESP_LOGI("WRITER TASK", "Value: \t %i", uxBits);
         vTaskDelay(100 / portTICK_PERIOD_MS);
         // Should not occur (only SensorBufferSdReady bit has been set)
      }
	}
}

extern "C" void app_main(void)
{
	nvs_flash_init();

   // xCreatedEventGroup = xEventGroupCreate();
   evg = xEventGroupCreate();

   if(evg == NULL)   {
      ESP_LOGE("ERROR", "Failed to create event group!");
   }

/*   TimerHandle_t wifi_poll_timer = NULL;
   wifi_poll_timer = xTimerCreate("sensor_poll_clock",  pdMS_TO_TICKS( 500 ), pdTRUE, 0,
   set_sensor_measurement_bit
   );
   xTimerStart( wifi_poll_timer, 0 );*/

    printf("\n\n\nHello world!\n");

    // Print chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    printf("silicon revision %d, ", chip_info.revision);
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024), (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // Start blink task
    //xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);

    // start sample task
    xTaskCreatePinnedToCore(&sample_task, "sample_task", 8192, NULL, 2, NULL, 1);

    // start writer task
    //xTaskCreatePinnedToCore(&writer_task, "writer_task", 4092, NULL, 5, NULL, 0);


    //xTaskCreatePinnedToCore(&wifi_task, "wifi_task", 10000, NULL, 0, NULL, 0);

    ESP_LOGI("MAIN", "Init done");
}
