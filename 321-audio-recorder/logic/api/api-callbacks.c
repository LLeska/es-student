#include "api/api-commands.h"
#include "api/api.h"
#include "platform/platform.h"
#include "telemetry-task/telemetry-task.h"
#include <errno.h>
#include <math.h>
#include "block-task/block-task.h"
#include "stream-task/stream-task.h"
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
    printf("button: %s, presses %u\n", button_task_is_pressed() ? "pressed" : "released",
           (unsigned)button_task_get_press_count());
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


static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    if (platform_adc_busy()) { printf("error: adc busy\n"); return; }
    printf("voltage: %.3f V\n", platform_voltage_read());
}
static void cmd_get_temp(const command_t *command)
{
    (void)command;
    if (platform_adc_busy()) { printf("error: adc busy\n"); return; }
    printf("temp: %.1f C\n", platform_temp_read());
}
void api_print_telemetry(const telemetry_record_t *record)
{
    printf("tm: %llu %.3f %.1f\n", (unsigned long long)record->time_us, record->voltage_v, record->temp_c);
}
static void cmd_tm_start(const command_t *command)
{
    uint32_t period = 100;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &period))) {
        printf("error: usage tm_start [<period_ms>]\n");
        return;
    }
    if (platform_adc_busy()) { printf("error: adc busy\n"); return; }
    if (!telemetry_task_start(period)) {
        printf("error: period_ms must be at least 10\n");
        return;
    }
    printf("telemetry: started, period %u ms\n", (unsigned)period);
}
static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\n");
}
static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\n");
}
static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\n");
}


static bool start_block(uint32_t count, block_task_callback_t callback)
{
    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN || !block_task_start(count, callback)) {
        printf("error: adc busy\n");
        return false;
    }
    return true;
}
static void cmd_timer_blink_start(const command_t *command)
{
    uint32_t period;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period)) { printf("error: usage timer_blink_start <period_ms>\n"); return; }
    if (!platform_blink_start(period)) { printf("error: period_ms must be 10..60000\n"); return; }
    led_task_set_state(LED_STATE_OFF);
    printf("timer_blink: started, period %u ms\n", (unsigned)period);
}
static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
    printf("timer_blink: stopped, %u toggles\n", (unsigned)platform_blink_stop());
}
static void cmd_cap_start(const command_t *command)
{
    uint32_t count, period;
    if (command->argc != 2 || !parse_u32(command->argv[0], &count) || !parse_u32(command->argv[1], &period)) {
        printf("error: usage cap_start <N> <period_us>\n"); return;
    }
    if (count == 0 || count > PLATFORM_CAPTURE_MAX || period < 10) {
        printf("error: N must be 1..2000, period_us at least 10\n"); return;
    }
    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN || !platform_capture_start(count, period)) {
        printf("error: adc busy\n"); return;
    }
    printf("capture: started, %u samples, period %u us\n", (unsigned)count, (unsigned)period);
}
static void cmd_cap_dump(const command_t *command)
{
    (void)command;
    if (!platform_capture_done()) { printf("error: no capture\n"); return; }
    platform_capture_t capture;
    platform_capture_get(&capture);
    printf("capture: %u samples, period %u us, interval %u..%u us, handler %.1f %%\n",
           (unsigned)capture.count, (unsigned)capture.period_us,
           (unsigned)capture.min_interval_us, (unsigned)capture.max_interval_us, capture.handler_percent);
    for (uint32_t i = 0; i < capture.count; ++i) {
        if (i % 16 == 0) printf("cap:");
        printf(" %u", (unsigned)capture.samples[i]);
        if (i % 16 == 15 || i+1 == capture.count) printf("\n");
    }
}
static void print_dma_cap(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    uint32_t sum = 0;
    uint16_t smallest = UINT16_MAX, largest = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (samples[i] < smallest) smallest = samples[i];
        if (samples[i] > largest) largest = samples[i];
        sum += samples[i];
    }
    uint32_t expected = (uint64_t)count * 1000000 / PLATFORM_SAMPLE_RATE_HZ;
    printf("dma_cap: %u samples in %u us, expected %u us, min %u max %u mean %.1f\n",
           (unsigned)count, (unsigned)time_us, (unsigned)expected,
           (unsigned)smallest, (unsigned)largest, (double)sum / count);
}
static void cmd_dma_cap(const command_t *command)
{
    uint32_t count;
    if (command->argc != 1 || !parse_u32(command->argv[0], &count)) { printf("error: usage dma_cap <N>\n"); return; }
    if (count == 0 || count > PLATFORM_BLOCK_MAX) { printf("error: N must be 1..8192\n"); return; }
    (void)start_block(count, print_dma_cap);
}
void api_send_frame(const stream_header_t *header, const uint8_t *data, uint32_t bytes)
{
    platform_write(header, sizeof(*header));
    platform_write(data, bytes);
}
static void cmd_stream_start(const command_t *command)
{
    uint32_t bits = 16;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &bits)) || (bits != 8 && bits != 16)) {
        printf("error: usage stream_start [8|16]\n"); return;
    }
    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN || !stream_task_start((uint8_t)bits)) {
        printf("error: adc busy\n"); return;
    }
    led_task_set_state(LED_STATE_OFF);
    (void)platform_blink_start(250);
    printf("stream: started, 44100 Hz, %u bit\n", (unsigned)bits);
}
static void cmd_stream_stop(const command_t *command)
{
    (void)command;
    if (stream_task_get_state() == STREAM_STATE_RUN) { stream_task_stop(); platform_blink_stop(); }
    printf("stream: stopped, %u frames, %u samples, %u overruns\n", (unsigned)stream_task_get_frames(),
           (unsigned)stream_task_get_samples(), (unsigned)stream_task_get_overruns());
}
static void cmd_generate_tone(const command_t *command)
{
    uint32_t hz;
    if (command->argc != 1 || !parse_u32(command->argv[0], &hz)) { printf("error: usage generate_tone <Hz>\n"); return; }
    if (!platform_tone_set(hz)) { printf("error: Hz must be 0 or 20..20000\n"); return; }
    if (hz == 0) printf("tone: off\n");
    else printf("tone: %u Hz\n", (unsigned)hz);
}
static void block_stats(const uint16_t *samples, uint32_t count, float *mean, float *rms, float *peak)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum += samples[i];
    int32_t center = (int32_t)(sum / count);
    uint64_t squares = 0;
    uint32_t largest = 0;
    for (uint32_t i = 0; i < count; ++i) {
        int32_t deviation = (int32_t)samples[i] - center;
        uint32_t size = (uint32_t)(deviation < 0 ? -deviation : deviation);
        squares += (uint64_t)(deviation * deviation);
        if (size > largest) largest = size;
    }
    *mean = (float)sum / count;
    *rms = sqrtf((float)squares / count);
    *peak = (float)largest;
}
static void print_mic_bias(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum += samples[i];
    printf("mic: bias %.3f V\n", platform_adc_volts((float)sum / count));
}
static void print_mic_level(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    float mean, rms, peak;
    block_stats(samples, count, &mean, &rms, &peak);
    printf("mic: level %.1f mV rms, peak %.1f mV, %u ms\n",
           platform_adc_volts(rms) * 1000, platform_adc_volts(peak) * 1000,
           (unsigned)((uint64_t)count * 1000 / PLATFORM_SAMPLE_RATE_HZ));
}
static void cmd_mic_bias(const command_t *command)
{
    (void)command;
    (void)start_block(4410, print_mic_bias);
}
static void cmd_mic_level(const command_t *command)
{
    uint32_t ms = 100;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &ms))) { printf("error: usage mic_level [<ms>]\n"); return; }
    uint64_t count = (uint64_t)ms * PLATFORM_SAMPLE_RATE_HZ / 1000;
    if (ms == 0 || ms > 185 || count > PLATFORM_BLOCK_MAX) { printf("error: ms must be 1..185\n"); return; }
    (void)start_block((uint32_t)count, print_mic_level);
}
static void cmd_wdt_enable(const command_t *command)
{
    uint32_t ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &ms)) { printf("error: usage wdt_enable <timeout_ms>\n"); return; }
    if (!platform_watchdog_enable(ms)) { printf("error: timeout_ms must be 1..8388\n"); return; }
    printf("wdt: enabled, timeout %u ms\n", (unsigned)ms);
}
static void cmd_hang(const command_t *command)
{
    (void)command;
    printf("hang: superloop stopped\n");
    while (1) {}
}
static void cmd_reset_reason(const command_t *command)
{
    (void)command;
    uint32_t restarts;
    bool watchdog = platform_watchdog_caused_reset(&restarts);
    printf("reset: %s, restarts %u\n", watchdog ? "watchdog" : "other", (unsigned)restarts);
}

const api_command_t api_commands[] = {
    {"timer_blink_start", "timer indicator period in ms", cmd_timer_blink_start},
    {"timer_blink_stop", "stop indicator and count toggles", cmd_timer_blink_stop},
    {"cap_start", "timer capture: cap_start <N> <period_us>", cmd_cap_start},
    {"cap_dump", "dump timer capture", cmd_cap_dump},
    {"dma_cap", "capture block: dma_cap <N>", cmd_dma_cap},
    {"stream_start", "start PCM stream: stream_start [8|16]", cmd_stream_start},
    {"stream_stop", "stop PCM stream", cmd_stream_stop},
    {"generate_tone", "PWM test tone: generate_tone <Hz>", cmd_generate_tone},
    {"mic_bias", "microphone DC bias", cmd_mic_bias},
    {"mic_level", "microphone RMS and peak: mic_level [<ms>]", cmd_mic_level},
    {"wdt_enable", "watchdog timeout: wdt_enable <timeout_ms>", cmd_wdt_enable},
    {"hang", "stop superloop to test watchdog", cmd_hang},
    {"reset_reason", "last reset cause", cmd_reset_reason},
    {"get_voltage", "voltage on GP26", cmd_get_voltage},
    {"get_temp", "chip temperature", cmd_get_temp},
    {"tm_start", "start telemetry: tm_start [<period_ms>]", cmd_tm_start},
    {"tm_stop", "stop telemetry", cmd_tm_stop},
    {"charge_on", "charge RC circuit on GP22", cmd_charge_on},
    {"charge_off", "discharge RC circuit", cmd_charge_off},
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
