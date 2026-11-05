#include "block-task/block-task.h"
#include <stddef.h>
#include "platform/platform.h"
static bool active;
static uint32_t count;
static block_task_callback_t callback;
bool block_task_start(uint32_t n, block_task_callback_t on_block)
{
    if (active || on_block == NULL || !platform_block_start(n)) return false;
    count = n;
    callback = on_block;
    active = true;
    return true;
}
void block_task_handle(void)
{
    if (!active || !platform_block_done()) return;
    block_task_callback_t done = callback;
    active = false;
    callback = NULL;
    done(platform_block_samples(), count, platform_block_time_us());
}
