#include "telemetry-task/telemetry-task.h"
#include <stddef.h>
#include "platform/platform.h"
#include "systime/systime.h"
static telemetry_state_t state;
static uint32_t period_ms;
static uint64_t last_us;
static telemetry_task_callback_t callback;
void telemetry_task_init(telemetry_task_callback_t on_record)
{
    callback = on_record;
    state = TELEMETRY_STATE_IDLE;
    period_ms = 100;
}
bool telemetry_task_start(uint32_t period)
{
    if (period < 10) return false;
    period_ms = period;
    last_us = systime_us() - (uint64_t)period_ms * 1000;
    state = TELEMETRY_STATE_RUN;
    return true;
}
void telemetry_task_stop(void) { state = TELEMETRY_STATE_IDLE; }
telemetry_state_t telemetry_task_get_state(void) { return state; }
uint32_t telemetry_task_get_period_ms(void) { return period_ms; }
void telemetry_task_handle(void)
{
    switch (state) {
    case TELEMETRY_STATE_IDLE: break;
    case TELEMETRY_STATE_RUN:
        if (systime_period_elapsed(&last_us, (uint64_t)period_ms * 1000)) {
            telemetry_record_t record = {
                .time_us = systime_us(),
                .voltage_v = platform_voltage_read(),
                .temp_c = platform_temp_read(),
            };
            if (callback != NULL) callback(&record);
        }
        break;
    }
}
