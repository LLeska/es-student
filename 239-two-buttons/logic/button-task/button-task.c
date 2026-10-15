#include "button-task/button-task.h"
#include <stddef.h>
#include "systime/systime.h"

typedef enum { RELEASED, PRESS_PENDING, PRESSED, RELEASE_PENDING } button_state_t;
typedef struct {
    button_state_t state;
    uint64_t changed_us;
    uint64_t pressed_us;
    uint32_t press_count;
    bool long_reported;
    button_task_callback_t callback;
    button_task_callback_t long_callback;
} button_t;
static const uint64_t DEBOUNCE_US = 20000;
static const uint64_t LONG_PRESS_US = 1000000;
static button_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button, button_task_callback_t on_press,
                      button_task_callback_t on_long_press)
{
    if (button < PLATFORM_BUTTON_COUNT) {
        buttons[button] = (button_t){
            .state = RELEASED,
            .changed_us = systime_us(),
            .callback = on_press,
            .long_callback = on_long_press,
        };
    }
}

void button_task_handle(void)
{
    uint64_t now_us = systime_us();
    for (uint32_t i = 0; i < PLATFORM_BUTTON_COUNT; ++i) {
        button_t *b = &buttons[i];
        bool pressed = platform_button_read((platform_button_t)i);
        switch (b->state) {
        case RELEASED:
            if (pressed) { b->state = PRESS_PENDING; b->changed_us = now_us; }
            break;
        case PRESS_PENDING:
            if (!pressed) b->state = RELEASED;
            else if (now_us - b->changed_us >= DEBOUNCE_US) {
                b->state = PRESSED;
                b->pressed_us = now_us;
                b->long_reported = false;
                ++b->press_count;
                if (b->long_callback == NULL && b->callback != NULL) b->callback();
            }
            break;
        case PRESSED:
            if (!pressed) { b->state = RELEASE_PENDING; b->changed_us = now_us; }
            break;
        case RELEASE_PENDING:
            if (pressed) b->state = PRESSED;
            else if (now_us - b->changed_us >= DEBOUNCE_US) {
                b->state = RELEASED;
                if (b->long_callback != NULL && !b->long_reported && b->callback != NULL) {
                    b->callback();
                }
            }
            break;
        }
        if (pressed && b->state == PRESSED && b->long_callback != NULL &&
            !b->long_reported && now_us - b->pressed_us >= LONG_PRESS_US) {
            b->long_reported = true;
            b->long_callback();
        }
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    return button < PLATFORM_BUTTON_COUNT &&
           (buttons[button].state == PRESSED || buttons[button].state == RELEASE_PENDING);
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    return button < PLATFORM_BUTTON_COUNT ? buttons[button].press_count : 0;
}
