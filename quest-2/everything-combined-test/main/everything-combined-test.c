
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "./ADXL343.h"

/*I2C*/
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           23
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define WRITE_BIT                   I2C_MASTER_WRITE
#define READ_BIT                    I2C_MASTER_READ
#define ACK_CHECK_EN                true
#define ACK_CHECK_DIS               false
#define ACK_VAL                     0x00
#define NACK_VAL                    0x01

#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS portTICK_RATE_MS
#endif


#define LIDAR_ADDR                  0x62

#define ACCEL_ADDR                  ADXL343_ADDRESS

#define DEFAULT_VREF                1100
#define NO_OF_SAMPLES               64
static const adc_channel_t  adc_channel = ADC_CHANNEL_6;   // GPIO34
static const adc_atten_t    adc_atten   = ADC_ATTEN_DB_0;
static const adc_unit_t     adc_unit    = ADC_UNIT_1;
static esp_adc_cal_characteristics_t *adc_chars;

//I2C stuff
static void i2c_master_init(void) {
    printf("\n>> I2C Config\n");
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_MASTER_SDA_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = I2C_MASTER_SCL_IO,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err == ESP_OK) { printf("- parameters: ok\n"); }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err == ESP_OK) { printf("- initialized: yes\n"); }
    i2c_set_data_mode(I2C_MASTER_NUM, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

static esp_err_t i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static uint8_t i2c_read_reg(uint8_t dev_addr, uint8_t reg) {
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data, ACK_CHECK_DIS);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return data;
}

static int16_t i2c_read16(uint8_t dev_addr, uint8_t reg) {
    uint8_t data[2];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data[0], ACK_VAL);
    i2c_master_read_byte(cmd, &data[1], NACK_VAL);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (int16_t)((data[1] << 8) | data[0]);
}

static void i2c_scanner(void) {
    printf("\n>> I2C scanning...\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        if (i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS) == ESP_OK) {
            printf("- Device found at address: 0x%X\n", i);
            count++;
        }
        i2c_cmd_link_delete(cmd);
    }
    if (count == 0) { printf("- No I2C devices found!\n"); }
}



//Lidar 
static int16_t lidar_read_distance(void) {
    uint8_t low  = i2c_read_reg(LIDAR_ADDR, 0x10);
    uint8_t high = i2c_read_reg(LIDAR_ADDR, 0x11);
    return (int16_t)((high << 8) | low);
}

static void lidar_task(void *pvParameters) {
    printf("LiDAR Task Started\n");
    while (1) {
        if (i2c_write_reg(LIDAR_ADDR, 0x00, 0x04) != ESP_OK) {
            printf("[LiDAR] Error sending measurement command\n");
        } else {
            uint8_t status;
            int timeout = 0;
            do {
                vTaskDelay(1);
                status = i2c_read_reg(LIDAR_ADDR, 0x01);
                timeout++;
            } while ((status & 0x01) && timeout < 50);

            if (timeout >= 50) {
                printf("[LiDAR] Sensor Timeout - Check Wiring\n");
            } else {
                int16_t distance = lidar_read_distance();
                if (distance < 0 || distance > 2000) {
                    printf("[LiDAR] Distance out of range or Error\n");
                } else {
                    printf("[LiDAR] Distance: %d cm\n", distance);
                }
            }
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

//Accelerometer
static void accel_set_range(range_t range) {
    uint8_t format = i2c_read_reg(ACCEL_ADDR, ADXL343_REG_DATA_FORMAT);
    format &= ~0x0F;
    format |= range;
    format |= 0x08;  // full resolution
    i2c_write_reg(ACCEL_ADDR, ADXL343_REG_DATA_FORMAT, format);
}

static range_t accel_get_range(void) {
    return (range_t)(i2c_read_reg(ACCEL_ADDR, ADXL343_REG_DATA_FORMAT) & 0x03);
}

static dataRate_t accel_get_data_rate(void) {
    return (dataRate_t)(i2c_read_reg(ACCEL_ADDR, ADXL343_REG_BW_RATE) & 0x0F);
}

static void accel_get_values(float *xp, float *yp, float *zp) {
    *xp = i2c_read16(ACCEL_ADDR, ADXL343_REG_DATAX0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *yp = i2c_read16(ACCEL_ADDR, ADXL343_REG_DATAY0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *zp = i2c_read16(ACCEL_ADDR, ADXL343_REG_DATAZ0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
}

static void accel_calc_roll_pitch(float x, float y, float z) {
    float roll  = atan2(y, z) * 180.0 / M_PI;
    float pitch = atan2(-x, sqrt((y * y) + (z * z))) * 180.0 / M_PI;
    printf("[ACCEL] Roll: %.2f\tPitch: %.2f\n", roll, pitch);
}

static void accel_task(void *pvParameters) {
    printf("Accelerometer Task Started\n");
    while (1) {
        float xVal, yVal, zVal;
        accel_get_values(&xVal, &yVal, &zVal);
        printf("[ACCEL] X: %.2f\tY: %.2f\tZ: %.2f m/s^2\n", xVal, yVal, zVal);
        accel_calc_roll_pitch(xVal, yVal, zVal);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}


//Ultrasonic
static void adc_init(void) {
    // Check eFuse calibration support
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc_channel, adc_atten);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        adc_unit, adc_atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("ADC: Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("ADC: Characterized using eFuse Vref\n");
    } else {
        printf("ADC: Characterized using Default Vref\n");
    }
}

static void ultrasonic_task(void *pvParameters) {
    printf("Ultrasonic Task Started\n");
    while (1) {
        uint32_t adc_reading = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            adc_reading += adc1_get_raw((adc1_channel_t)adc_channel);
        }
        adc_reading /= NO_OF_SAMPLES;

        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        // MB1010 LV-MaxSonar: Vcc/512 per inch → at 3.3V: ~6.4mV/inch → convert to cm
        float distance_cm = (voltage / 6.4f) * 2.54f;
        printf("[SONIC] Raw: %lu\tVoltage: %lumV\tDistance: %.1f cm\n",
               adc_reading, voltage, distance_cm);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



//Main function
void app_main(void) {

    // --- Shared I2C bus init ---
    i2c_master_init();
    i2c_scanner();

    // --- ADXL343 setup ---
    uint8_t deviceID = i2c_read_reg(ACCEL_ADDR, ADXL343_REG_DEVID);
    if (deviceID == 0xE5) {
        printf("\n>> Found ADXL343\n");
    } else {
        printf("\n>> ADXL343 NOT found (got 0x%X)\n", deviceID);
    }
    i2c_write_reg(ACCEL_ADDR, ADXL343_REG_INT_ENABLE, 0);   // disable interrupts
    accel_set_range(ADXL343_RANGE_16_G);
    printf("- Accel Range: +/- 16 g\n");
    printf("- Accel Data Rate: %d (raw enum)\n", accel_get_data_rate());
    i2c_write_reg(ACCEL_ADDR, ADXL343_REG_POWER_CTL, 0x08); // enable measurement mode

    // --- ADC / Ultrasonic init ---
    adc_init();

    // --- Short stabilization delay ---
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // --- Launch sensor tasks ---
    xTaskCreate(lidar_task,       "lidar_task",   4096, NULL, 5, NULL);
    xTaskCreate(accel_task,       "accel_task",   4096, NULL, 5, NULL);
    xTaskCreate(ultrasonic_task,  "sonic_task",   4096, NULL, 5, NULL);
}
