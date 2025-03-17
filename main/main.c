/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"

const int ECHO_PIN = 15;
const int TRIGGER_PIN = 14;

volatile int time_end = 0;
volatile int time_start = 0;
static volatile bool fired = false;
volatile int echo_flag = 0;

// Callback do timer repetitivo
bool alarm_callback(repeating_timer_t *rt) {
    fired = true;
    return false;
}

// Callback do GPIO (sensor ultrassônico)
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            time_start = to_us_since_boot(get_absolute_time());
        } else if (events & GPIO_IRQ_EDGE_FALL) {
            time_end = to_us_since_boot(get_absolute_time());
            echo_flag = 1;
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
    echo_flag = 0;
}

int main() {
    stdio_init_all();

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_pull_down(ECHO_PIN);
    gpio_set_irq_enabled_with_callback(
        ECHO_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &gpio_callback
    );

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    repeating_timer_t timer_r;
    bool timer_active = false;

    
    datetime_t t = {
        .year  = 2024,
        .month = 3,
        .day   = 16,
        .dotw  = 0,
        .hour  = 23,
        .min   = 30,
        .sec   = 00
    };
    rtc_init();
    rtc_set_datetime(&t);

    char buffer[32];
    int index = 0;

    printf("Sistema iniciado. Digite 'Start' ou 'Stop' e aperte Enter.\n");

    while (true) {
        int ch = getchar_timeout_us(1000);
    
        if (ch != PICO_ERROR_TIMEOUT) {
            
            putchar(ch);
            
            if (ch == '\n' || ch == '\r') {
                
                buffer[index] = '\0';
                index = 0;
    
                if (strcmp(buffer, "Start") == 0) {
                    if (!timer_active) {
                        add_repeating_timer_ms(1000, alarm_callback, NULL, &timer_r);
                        timer_active = true;
                        printf("Timer ativado.\n");
                    } else {
                        printf("Timer já ativado.\n");
                    }
                } else if (strcmp(buffer, "Stop") == 0) {
                    if (timer_active) {
                        cancel_repeating_timer(&timer_r);
                        timer_active = false;
                        printf("Timer desativado.\n");
                    } else {
                        printf("Timer já desativado.\n");
                    }
                } else {
                    printf("Comando inválido.\n");
                }
            } else {
                
                if (index < sizeof(buffer) - 1) {
                    buffer[index++] = ch;
                }
            }
        }
    
        if (fired) {
            fired = false;
    
            if (!echo_flag) {
                printf("Erro no sensor!\n");
                add_repeating_timer_ms(1000, alarm_callback, NULL, &timer_r);
                continue;
            }

            trigger_sensor();
    
            int delta_T = time_end - time_start;
            float cm = (delta_T / 2.0f) * 0.0343f;
    
            print_rtc_time();
            printf("Distância medida: %.2f cm\n", cm);
    
            add_repeating_timer_ms(1000, alarm_callback, NULL, &timer_r);
        }
    }
    

    return 0;
}
