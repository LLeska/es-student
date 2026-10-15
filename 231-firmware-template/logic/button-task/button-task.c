#include "button-task/button-task.h"
#include <stddef.h>
#include "platform/platform.h"
#include "systime/systime.h"

typedef enum { RELEASED, PRESS_PENDING, PRESSED, RELEASE_PENDING } button_state_t;
static const uint64_t DEBOUNCE_US = 20000;
static button_state_t state;
static uint64_t changed_us;
static uint32_t press_count;
static button_task_callback_t callback;

void button_task_init(button_task_callback_t on_press)
{
    state = RELEASED;
    changed_us = systime_us();
    press_count = 0;
    callback = on_press;
}
void button_task_handle(void)
{
    bool pressed = platform_button_read();
    uint64_t now_us = systime_us();
    switch (state) {
    case RELEASED:
        if (pressed) { state = PRESS_PENDING; changed_us = now_us; }
        break;
    case PRESS_PENDING:
        if (!pressed) state = RELEASED;
        else if (now_us - changed_us >= DEBOUNCE_US) {
            state = PRESSED;
            ++press_count;
            if (callback != NULL) callback();
        }
        break;
    case PRESSED:
        if (!pressed) { state = RELEASE_PENDING; changed_us = now_us; }
        break;
    case RELEASE_PENDING:
        if (pressed) state = PRESSED;
        else if (now_us - changed_us >= DEBOUNCE_US) state = RELEASED;
        break;
    }
}
bool button_task_is_pressed(void) { return state == PRESSED || state == RELEASE_PENDING; }
uint32_t button_task_get_press_count(void) { return press_count; }
