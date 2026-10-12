/**
 * @file motor_driver.h
 * @brief DRV8833 dual H-bridge motor driver for rover thrusters
 */

#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"

typedef struct {
    TIM_HandleTypeDef *htim;
    bool initialised;
    bool sleeping;
} motor_driver_t;

int  motor_driver_init(motor_driver_t *md, TIM_HandleTypeDef *htim);
void motor_set_speeds(motor_driver_t *md, int16_t left, int16_t right);
void motor_stop(motor_driver_t *md);
void motor_sleep(motor_driver_t *md);
void motor_wake(motor_driver_t *md);

#endif /* MOTOR_DRIVER_H */
