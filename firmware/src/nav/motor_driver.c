/**
 * @file motor_driver.c
 * @brief DRV8833 motor driver implementation
 *
 * Controls two thrusters via PWM for differential steering.
 * Speed range: -1000 to +1000 (PWM duty cycle mapping).
 */

#include "nav/motor_driver.h"
#include "pin_map.h"

#ifdef BUILD_MODE_ROVER

#define PWM_PERIOD  999     /* Timer auto-reload value → 0–999 duty */

int motor_driver_init(motor_driver_t *md, TIM_HandleTypeDef *htim)
{
    if (!md || !htim) return -1;

    md->htim = htim;
    md->sleeping = false;

    /* Start all PWM channels */
    HAL_TIM_PWM_Start(htim, MOTOR_LEFT_FWD_CH);
    HAL_TIM_PWM_Start(htim, MOTOR_LEFT_REV_CH);
    HAL_TIM_PWM_Start(htim, MOTOR_RIGHT_FWD_CH);
    HAL_TIM_PWM_Start(htim, MOTOR_RIGHT_REV_CH);

    /* Enable driver (pull SLEEP pin high) */
    HAL_GPIO_WritePin(MOTOR_SLEEP_PORT, MOTOR_SLEEP_PIN, GPIO_PIN_SET);

    motor_stop(md);

    md->initialised = true;
    return 0;
}

/**
 * @brief Set motor speeds for differential drive
 * @param left   Left thruster: -1000 (full reverse) to +1000 (full forward)
 * @param right  Right thruster: -1000 to +1000
 */
void motor_set_speeds(motor_driver_t *md, int16_t left, int16_t right)
{
    if (!md || !md->initialised || md->sleeping) return;

    /* Clamp values */
    if (left > 1000)  left = 1000;
    if (left < -1000) left = -1000;
    if (right > 1000)  right = 1000;
    if (right < -1000) right = -1000;

    /* Left motor */
    if (left >= 0) {
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_FWD_CH, (uint32_t)left);
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_REV_CH, 0);
    } else {
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_FWD_CH, 0);
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_REV_CH, (uint32_t)(-left));
    }

    /* Right motor */
    if (right >= 0) {
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_FWD_CH, (uint32_t)right);
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_REV_CH, 0);
    } else {
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_FWD_CH, 0);
        __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_REV_CH, (uint32_t)(-right));
    }
}

void motor_stop(motor_driver_t *md)
{
    if (!md || !md->initialised) return;
    __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_FWD_CH, 0);
    __HAL_TIM_SET_COMPARE(md->htim, MOTOR_LEFT_REV_CH, 0);
    __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_FWD_CH, 0);
    __HAL_TIM_SET_COMPARE(md->htim, MOTOR_RIGHT_REV_CH, 0);
}

void motor_sleep(motor_driver_t *md)
{
    if (!md) return;
    motor_stop(md);
    HAL_GPIO_WritePin(MOTOR_SLEEP_PORT, MOTOR_SLEEP_PIN, GPIO_PIN_RESET);
    md->sleeping = true;
}

void motor_wake(motor_driver_t *md)
{
    if (!md) return;
    HAL_GPIO_WritePin(MOTOR_SLEEP_PORT, MOTOR_SLEEP_PIN, GPIO_PIN_SET);
    HAL_Delay(1);  /* DRV8833 wake-up time */
    md->sleeping = false;
}

#endif /* BUILD_MODE_ROVER */
