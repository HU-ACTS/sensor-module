#include "Bmp280Implementation.hpp"

//#define TAG_BMP280 "BMP280"

#define SCL_PIN GPIO_NUM_26
#define SDA_PIN GPIO_NUM_25

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

s8 BMP280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
   s32 iError = BMP280_INIT_VALUE;

   esp_err_t espRc;
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();

   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

   i2c_master_write_byte(cmd, reg_addr, true);
   i2c_master_write(cmd, reg_data, cnt, true);
   i2c_master_stop(cmd);

   espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
   if (espRc == ESP_OK) {
      iError = SUCCESS;
   } else {
      iError = FAIL;
   }
   i2c_cmd_link_delete(cmd);

   return (s8)iError;
}

s8 BMP280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
   s32 iError = BMP280_INIT_VALUE;
   esp_err_t espRc;

   i2c_cmd_handle_t cmd = i2c_cmd_link_create();

   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write_byte(cmd, reg_addr, true);

   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

   if (cnt > 1) {
      i2c_master_read(cmd, reg_data, cnt-1, I2C_MASTER_ACK);
   }
   i2c_master_read_byte(cmd, reg_data+cnt-1, I2C_MASTER_NACK);
   i2c_master_stop(cmd);

   espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
   if (espRc == ESP_OK) {
      iError = SUCCESS;
   } else {
      iError = FAIL;
   }

   i2c_cmd_link_delete(cmd);

   return (s8)iError;
}

void BMP280_delay_msek(u32 msek)
{
   vTaskDelay(msek/portTICK_PERIOD_MS);
}

Bmp280Implementation::Bmp280Implementation() {
	int com_result;
	memset(BMPData, 0, sizeof(int) * 2);
	memset(BackupBMPData, 0, sizeof(int) * 2);

	bmp280_com_functions.bus_write = BMP280_I2C_bus_write;
	bmp280_com_functions.bus_read = BMP280_I2C_bus_read;
	bmp280_com_functions.dev_addr = BMP280_I2C_ADDRESS2;
	bmp280_com_functions.delay_msec = BMP280_delay_msek;

	if(bmp280_init(&bmp280_com_functions) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 1");
		return;
	}

	if(bmp280_set_oversamp_pressure(BMP280_OVERSAMP_16X) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 2");
		return;
	}

	if(bmp280_set_oversamp_temperature(BMP280_OVERSAMP_2X) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 3");
		return;
	}

	if(bmp280_set_standby_durn(BMP280_STANDBY_TIME_1_MS) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 4");
		return;
	}

	if(bmp280_set_filter(BMP280_FILTER_COEFF_16) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 5");
		return;
	}

	if(bmp280_set_power_mode(BMP280_NORMAL_MODE) != SUCCESS) {
		IsInitialized = false;
		ESP_LOGI(TAG_BMP280, "BMP not discovered, error 6");
		return;
	}

	IsInitialized = true;
	ESP_LOGI(TAG_BMP280, "BMP init ok");
}

unsigned short* Bmp280Implementation::SensorRead() {

	s32 v_uncomp_pressure_s32;
	s32 v_uncomp_temperature_s32;

	memset(BMPData, 0, sizeof(int) * 2);

	if (IsInitialized == true) {
		int com_result = bmp280_read_uncomp_pressure_temperature(&v_uncomp_pressure_s32, &v_uncomp_temperature_s32);
		if (com_result == SUCCESS) {

			int temperature = (int)bmp280_compensate_temperature_double(v_uncomp_temperature_s32);
			int pressure = (int)bmp280_compensate_pressure_double(v_uncomp_pressure_s32);

			memcpy(BMPData, &temperature, sizeof(int));
			memcpy(&BMPData[2], &pressure, sizeof(int));
			memcpy(BackupBMPData, BMPData, sizeof(int) * 2);

			//ESP_LOGI(TAG_BMP280, "%d C \t %d hPa ", temperature, pressure);
		}
		else {
			memcpy(BMPData, BackupBMPData, sizeof(int) * 2);
			ESP_LOGE(TAG_BMP280, "Measure error, code: %d", com_result);
		}
	}

	return &BMPData[0];
}
