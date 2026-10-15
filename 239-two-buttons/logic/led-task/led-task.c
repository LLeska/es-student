#include "led-task/led-task.h"
#include "platform/platform.h"
#include "systime/systime.h"

static led_state_t led_state;
static uint32_t period_ms;
static uint64_t last_us;
static bool output_on;

void led_task_set_state(led_state_t state)
{
    led_state = state;
    switch (state) {
    case LED_STATE_OFF: output_on = false; break;
    case LED_STATE_ON: output_on = true; break;
    case LED_STATE_BLINK:
        output_on = !output_on;
        last_us = systime_us();
        break;
    }
    platform_led_set(output_on);
}

void led_task_init(void)
{
    period_ms = 1000;
    output_on = false;
    led_task_set_state(LED_STATE_BLINK);
}

void led_task_handle(void)
{
    switch (led_state) {
    case LED_STATE_OFF:
    case LED_STATE_ON: break;
    case LED_STATE_BLINK:
        if (systime_period_elapsed(&last_us, (uint64_t)period_ms * 1000 / 2)) {
            output_on = !output_on;
            platform_led_set(output_on);
        }
        break;
    }
}

led_state_t led_task_get_state(void) { return led_state; }
uint32_t led_task_get_period_ms(void) { return period_ms; }
bool led_task_set_period_ms(uint32_t value)
{
    if (value == 0) return false;
    period_ms = value;
    return true;
}
void led_task_next_state(void)
{
    led_task_set_state((led_state_t)((led_state + 1) % 3));
}

void led_task_next_period(void)
{
    switch (period_ms) {
    case 250: (void)led_task_set_period_ms(500); break;
    case 500: (void)led_task_set_period_ms(1000); break;
    default: (void)led_task_set_period_ms(250); break;
    }
}

void led_task_blink(void)
{
    led_task_set_state(LED_STATE_BLINK);
}
