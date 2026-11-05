#include "platform/platform.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"

#define ADC_CLOCK_HZ 48000000.0f
#define TONE_PIN 20
#define TONE_COUNT_HZ 1000000
#define WATCHDOG_MAX_MS 8388
typedef enum { ADC_FREE, ADC_CAPTURE, ADC_BLOCK, ADC_STREAM } adc_user_t;
static volatile adc_user_t adc_user;
static repeating_timer_t blink_timer;
static bool blink_running;
static volatile bool blink_on;
static volatile uint32_t blink_toggles;
static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static uint32_t capture_count;
static uint32_t capture_period_us;
static volatile uint32_t capture_index;
static volatile uint32_t capture_handler_us;
static int block_channel;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_start_us;
static volatile uint32_t block_end_us;
static volatile bool block_ready;
static int stream_channels[2];
static uint8_t stream_buffers[2][PLATFORM_STREAM_BYTES] __attribute__((aligned(4)));
static volatile bool stream_ready[2];
static volatile uint32_t stream_overruns;
static uint32_t stream_samples;
static uint32_t stream_next;
static uint tone_slice;
static bool watchdog_reset;
static uint32_t watchdog_restarts;
static void on_block_done(void);
static void on_stream_buffer(void);

#define ADC_PIN 26
#define ADC_CHANNEL 0
#define ADC_TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096
#define CHARGE_PIN 22

static const uint LED_PIN = 25;
static const uint BUTTON_PIN = 15;

void platform_init(void)
{
    watchdog_reset = watchdog_enable_caused_reboot();
    if (watchdog_reset) watchdog_hw->scratch[0]++;
    else watchdog_hw->scratch[0] = 0;
    watchdog_restarts = watchdog_hw->scratch[0];
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_init(CHARGE_PIN);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_put(CHARGE_PIN, false);
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
    stream_channels[0] = dma_claim_unused_channel(true);
    stream_channels[1] = dma_claim_unused_channel(true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_stream_buffer);
    irq_set_enabled(DMA_IRQ_0, true);
    gpio_set_function(TONE_PIN, GPIO_FUNC_PWM);
    tone_slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_set_clkdiv(tone_slice, (float)clock_get_hz(clk_sys) / TONE_COUNT_HZ);
    pwm_set_wrap(tone_slice, 999);
    pwm_set_gpio_level(TONE_PIN, 0);
    pwm_set_enabled(tone_slice, true);
}
void platform_led_set(bool on) { gpio_put(LED_PIN, on); }
bool platform_button_read(void) { return !gpio_get(BUTTON_PIN); }

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}
float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);
    float voltage = adc_read() * ADC_VREF_V / ADC_LEVELS;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}
void platform_charge_set(bool on) { gpio_put(CHARGE_PIN, on); }

static bool on_blink(repeating_timer_t *timer)
{
    (void)timer;
    blink_on = !blink_on;
    gpio_put(LED_PIN, blink_on);
    ++blink_toggles;
    return true;
}
bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10 || period_ms > 60000) return false;
    platform_blink_stop();
    blink_toggles = 0;
    blink_on = false;
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}
uint32_t platform_blink_stop(void)
{
    if (blink_running) cancel_repeating_timer(&blink_timer);
    blink_running = false;
    gpio_put(LED_PIN, false);
    return blink_toggles;
}
bool platform_adc_busy(void) { return adc_user != ADC_FREE; }
static bool on_capture(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t start = time_us_32();
    capture_times[capture_index] = start;
    capture_samples[capture_index] = adc_read();
    ++capture_index;
    capture_handler_us += time_us_32() - start;
    if (capture_index < capture_count) return true;
    adc_user = ADC_FREE;
    return false;
}
bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10) return false;
    adc_select_input(ADC_CHANNEL);
    capture_count = count;
    capture_period_us = period_us;
    capture_index = 0;
    capture_handler_us = 0;
    adc_user = ADC_CAPTURE;
    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer)) {
        adc_user = ADC_FREE;
        return false;
    }
    return true;
}
bool platform_capture_done(void) { return capture_count != 0 && capture_index == capture_count; }
void platform_capture_get(platform_capture_t *capture)
{
    *capture = (platform_capture_t){.samples = capture_samples, .count = capture_count, .period_us = capture_period_us};
    if (capture_count < 2) return;
    uint32_t smallest = UINT32_MAX, largest = 0;
    for (uint32_t i = 1; i < capture_count; ++i) {
        uint32_t dt = capture_times[i] - capture_times[i-1];
        if (dt < smallest) smallest = dt;
        if (dt > largest) largest = dt;
    }
    uint32_t duration = capture_times[capture_count-1] - capture_times[0];
    capture->min_interval_us = smallest;
    capture->max_interval_us = largest;
    capture->handler_percent = duration ? 100.0f * capture_handler_us / duration : 0.0f;
}
static void adc_free_running(bool bits8)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, bits8);
    adc_set_clkdiv(ADC_CLOCK_HZ / PLATFORM_SAMPLE_RATE_HZ - 1);
}
static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}
static void on_block_done(void)
{
    if (!dma_channel_get_irq1_status(block_channel)) return;
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}
bool platform_block_start(uint32_t count)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_BLOCK_MAX) return false;
    adc_free_running(false);
    dma_channel_config cfg = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    block_ready = false;
    adc_user = ADC_BLOCK;
    dma_channel_configure(block_channel, &cfg, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);
    return true;
}
bool platform_block_done(void) { return block_ready; }
const uint16_t *platform_block_samples(void) { return block_samples; }
uint32_t platform_block_time_us(void) { return block_end_us - block_start_us; }
static void on_stream_buffer(void)
{
    for (int i = 0; i < 2; ++i) {
        if (dma_channel_get_irq0_status(stream_channels[i])) {
            dma_channel_acknowledge_irq0(stream_channels[i]);
            dma_channel_set_write_addr(stream_channels[i], stream_buffers[i], false);
            if (stream_ready[(i+1)%2]) ++stream_overruns;
            stream_ready[i] = true;
        }
    }
}
bool platform_stream_start(uint8_t bits)
{
    if (platform_adc_busy() || (bits != 8 && bits != 16)) return false;
    stream_samples = bits == 8 ? PLATFORM_STREAM_BYTES : PLATFORM_STREAM_BYTES / 2;
    adc_free_running(bits == 8);
    for (int i = 0; i < 2; ++i) {
        dma_channel_config cfg = dma_channel_get_default_config(stream_channels[i]);
        channel_config_set_transfer_data_size(&cfg, bits == 8 ? DMA_SIZE_8 : DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);
        channel_config_set_chain_to(&cfg, stream_channels[(i+1)%2]);
        dma_channel_acknowledge_irq0(stream_channels[i]);
        dma_channel_configure(stream_channels[i], &cfg, stream_buffers[i], &adc_hw->fifo, stream_samples, false);
        dma_channel_set_irq0_enabled(stream_channels[i], true);
        stream_ready[i] = false;
    }
    stream_overruns = 0;
    stream_next = 0;
    adc_user = ADC_STREAM;
    dma_channel_start(stream_channels[0]);
    adc_run(true);
    return true;
}
void platform_stream_stop(void)
{
    if (adc_user != ADC_STREAM) return;
    adc_run(false);
    // RP2040-E13: break both chain links before aborting either channel.
    for (int i = 0; i < 2; ++i) {
        dma_channel_set_irq0_enabled(stream_channels[i], false);
        dma_channel_config cfg = dma_get_channel_config(stream_channels[i]);
        channel_config_set_chain_to(&cfg, stream_channels[i]);
        dma_channel_set_config(stream_channels[i], &cfg, false);
    }
    for (int i = 0; i < 2; ++i) {
        dma_channel_abort(stream_channels[i]);
        dma_channel_acknowledge_irq0(stream_channels[i]);
    }
    adc_one_shot();
    adc_user = ADC_FREE;
}
const uint8_t *platform_stream_next(uint32_t *samples)
{
    if (adc_user != ADC_STREAM || !stream_ready[stream_next]) return NULL;
    *samples = stream_samples;
    return stream_buffers[stream_next];
}
void platform_stream_release(void)
{
    stream_ready[stream_next] = false;
    stream_next = (stream_next+1)%2;
}
uint32_t platform_stream_overruns(void) { return stream_overruns; }
void platform_write(const void *data, uint32_t bytes)
{
    stdio_put_string((const char *)data, (int)bytes, false, false);
}
bool platform_tone_set(uint32_t freq_hz)
{
    if (freq_hz == 0) { pwm_set_gpio_level(TONE_PIN, 0); return true; }
    if (freq_hz < 20 || freq_hz > 20000) return false;
    uint32_t period = TONE_COUNT_HZ / freq_hz;
    pwm_set_wrap(tone_slice, (uint16_t)(period-1));
    pwm_set_gpio_level(TONE_PIN, (uint16_t)(period/2));
    return true;
}
float platform_adc_volts(float code) { return code * ADC_VREF_V / ADC_LEVELS; }
bool platform_watchdog_enable(uint32_t timeout_ms)
{
    if (timeout_ms == 0 || timeout_ms > WATCHDOG_MAX_MS) return false;
    watchdog_enable(timeout_ms, true);
    return true;
}
void platform_watchdog_feed(void) { watchdog_update(); }
bool platform_watchdog_caused_reset(uint32_t *restarts)
{
    *restarts = watchdog_restarts;
    return watchdog_reset;
}
