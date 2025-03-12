/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include <string.h>

const int ECHO_PIN = 15;
const int TRIGGER_PIN = 14;

volatile int time_end = 0;
volatile int time_start = 0;
static volatile bool fired = false;
volatile int echo_rise = 0;

bool alarm_callback(repeating_timer_t *rt) {
    fired = 1;
    return true; // keep repeating    
}

void read_string_with_timeout(char *string, int max_length, uint32_t timeout) {
    int i;
    for (i = 0; i < max_length - 1; i++) {
        int character = getchar_timeout_us(timeout);
        if (character == PICO_ERROR_TIMEOUT) {
            break;
        }
        if (character == '\n') {
            break;
        }
        string[i++] = (char) character;
    }
    string[i] = '\0';
}

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            time_start = to_us_since_boot(get_absolute_time());
            echo_rise = 1;
        } else if (events & GPIO_IRQ_EDGE_FALL) {
            time_end = to_us_since_boot(get_absolute_time());
            
        }
    }
}

void print_rtc_time() {
    datetime_t current_time;
    rtc_get_datetime(&current_time);
    
    char datetime_buf[64];
    datetime_to_str(datetime_buf, sizeof(datetime_buf), &current_time);
    
    printf("[%s] ", datetime_buf);
}

void trigger_sensor() {
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);
}

int main() {
    stdio_init_all();

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
        &gpio_callback);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    repeating_timer_t timer_r;

    // **Inicializa o RTC**
    datetime_t t = {
        .year  = 2024,
        .month = 3,
        .day   = 12,
        .dotw  = 2, // 0 = Domingo, 2 = Terça-feira
        .hour  = 11,
        .min   = 20,
        .sec   = 00
    };
    rtc_init();
    rtc_set_datetime(&t);

    char message[10];

    while (true) {

        read_string_with_timeout(message, 10, 2000000);

        //if (strcmp(message, "Start") == 0) {
        
        trigger_sensor();
        if (!add_repeating_timer_us(500000, 
                    alarm_callback,
                    NULL, 
                    &timer_r)) {
            printf("Failed to add RED timer\n");
        }

        if (fired) {
            //fired = 0;       
            int delta_T = time_end - time_start;
            float cm = (delta_T/ 2) * 0.0343;
            print_rtc_time();
            printf("Distância medida: %.2f cm\n", cm);
            sleep_ms(100);
        }

        if (echo_rise) {
            cancel_repeating_timer(&timer_r);
        }

        //}

        if (strcmp(message, "Stop") == 0) {
            cancel_repeating_timer(&timer_r);
        }
    }
}
