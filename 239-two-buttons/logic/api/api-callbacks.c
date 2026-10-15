#include "api/api-commands.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "firmware.h"
#include "device/device.h"
#include "systime/systime.h"
#include "profiling/profiling.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"

static bool parse_u32(const char *text, uint32_t *result)
{
    if (*text == '\0') return false;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    errno = 0;
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value > UINT32_MAX) return false;
    *result = (uint32_t)value;
    return true;
}

static void cmd_info(const command_t *command)
{
    (void)command;
    device_info_t info;
    device_get_info(&info);
    printf("name: %s\nversion: %s\nproject: %s\nrepo: %s\n",
           FIRMWARE_NAME, FIRMWARE_VERSION, FIRMWARE_PROJECT, FIRMWARE_REPO);
    printf("board: %s\nserial: %s\n", info.board, info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n",
           (unsigned)info.manufacturer, (unsigned)info.part, (unsigned)info.revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}
static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n", (unsigned long long)systime_ms());
}
static void led_reply(void)
{
    const char *state = "off";
    switch (led_task_get_state()) {
    case LED_STATE_OFF: state = "off"; break;
    case LED_STATE_ON: state = "on"; break;
    case LED_STATE_BLINK: state = "blink"; break;
    }
    printf("led: %s, period %u ms\n", state, (unsigned)led_task_get_period_ms());
}
static void cmd_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    led_reply();
}
static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    led_reply();
}
static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    led_reply();
}
static void cmd_led_period(const command_t *command)
{
    uint32_t period;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period)) {
        printf("error: usage led_period <period_ms>\n");
        return;
    }
    if (!led_task_set_period_ms(period)) {
        printf("error: period_ms must be greater than 0\n");
        return;
    }
    led_reply();
}
static void cmd_button(const command_t *command)
{
    (void)command;
    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; ++i) {
        platform_button_t button = (platform_button_t)i;
        printf("button %u: %s, presses %u\n", (unsigned)i + 1,
               button_task_is_pressed(button) ? "pressed" : "released",
               (unsigned)button_task_get_press_count(button));
    }
}
static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &terms))) {
        printf("error: usage pi_start [<terms>]\n");
        return;
    }
    if (terms == 0) {
        printf("error: terms must be greater than 0\n");
        return;
    }
    if (!pi_task_start(terms)) {
        printf("error: pi is busy\n");
        return;
    }
    printf("pi: started, %u terms\n", (unsigned)terms);
}
static void cmd_pi(const command_t *command)
{
    (void)command;
    pi_result_t result;
    pi_task_get_result(&result);
    switch (result.state) {
    case PI_NOT_STARTED: printf("pi: not started\n"); break;
    case PI_RUNNING: printf("pi: running\n"); break;
    case PI_READY: printf("pi: %.9f in %llu ms\n", result.value, (unsigned long long)result.elapsed_ms); break;
    }
}
static void cmd_profiling(const command_t *command)
{
    (void)command;
#if PROFILING_ENABLED
    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; ++i) {
        profiling_result_t result;
        if (profiling_get(i, &result)) {
            printf("%-10s count %u mean %.2f us max %u us\n", result.name,
                   (unsigned)result.count, result.mean_us, (unsigned)result.max_us);
        }
    }
#else
    printf("profiling is disabled in this build\n");
#endif
}
static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    profiling_reset();
    printf("profiling reset\n");
}

const api_command_t api_commands[] = {
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime},
    {"led_enable", "turn LED on", cmd_led_enable},
    {"led_disable", "turn LED off", cmd_led_disable},
    {"led_blink", "blink LED", cmd_led_blink},
    {"led_period", "set blink period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button},
    {"pi_start", "start pi calculation: pi_start [<terms>]", cmd_pi_start},
    {"pi", "pi calculation state and result", cmd_pi},
    {"profiling", "stopwatch results", cmd_profiling},
    {"profiling_reset", "reset stopwatch results", cmd_profiling_reset},
};
const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);
