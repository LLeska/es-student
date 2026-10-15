#include "platform/platform.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

static const uint LED_PIN = 25;
static const uint BUTTON_PIN = 15;

void platform_init(void)
{
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
}
void platform_led_set(bool on) { gpio_put(LED_PIN, on); }
bool platform_button_read(void) { return !gpio_get(BUTTON_PIN); }
