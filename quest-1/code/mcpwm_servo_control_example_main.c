/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "example";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO1_MIN_PULSEWIDTH_US 450  // Minimum pulse width in microsecond
#define SERVO1_MAX_PULSEWIDTH_US 2050  // Maximum pulse width in microsecond
#define SERVO2_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO2_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -180   // Minimum angle
#define SERVO_MAX_DEGREE        180    // Maximum angle

#define SERVO_PULSE1_GPIO             12        // GPIO connects to the PWM signal line
#define SERVO_PULSE2_GPIO             14        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms


static inline uint32_t example1_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO1_MAX_PULSEWIDTH_US - SERVO1_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO1_MIN_PULSEWIDTH_US;
}

static inline uint32_t example2_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO2_MAX_PULSEWIDTH_US - SERVO2_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO2_MIN_PULSEWIDTH_US;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));


    //Servo 1
    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    mcpwm_cmpr_handle_t comparator1 = NULL;
    mcpwm_comparator_config_t comparator_config1 = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config1, &comparator1));

    mcpwm_gen_handle_t generator1 = NULL;
    mcpwm_generator_config_t generator_config1 = {
        .gen_gpio_num = SERVO_PULSE1_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config1, &generator1));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator1, example1_angle_to_compare(-90)));


    //Servo 2
    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    mcpwm_cmpr_handle_t comparator2 = NULL;
    mcpwm_comparator_config_t comparator_config2 = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config2, &comparator2));

    mcpwm_gen_handle_t generator2 = NULL;
    mcpwm_generator_config_t generator_config2 = {
        .gen_gpio_num = SERVO_PULSE2_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config2, &generator2));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator2, example2_angle_to_compare(-90)));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator1,
                                                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator1,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator1, MCPWM_GEN_ACTION_LOW)));
     //go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator2,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator2,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator2, MCPWM_GEN_ACTION_LOW)));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    


    int start_angle1 = -90;
    int start_angle2 = -90;
    int angle_switch = 60;
    //int step = 2;
    while (1) { 
        //ESP_LOGI(TAG, "Angle of rotation: %d", angle);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator1, example1_angle_to_compare(angle_switch - start_angle1)));
        angle_switch *= -1;
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator2, example2_angle_to_compare(angle_switch + start_angle2)));
        //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
        vTaskDelay(pdMS_TO_TICKS(1500));
       // if ((angle + step) > 60 || (angle + step) < -60) {
        //    step *= -1;
        //}
       // angle += step;
        }
      
}    