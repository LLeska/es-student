#include "stream-task/stream-task.h"
#include <stddef.h>
#include "platform/platform.h"
static stream_state_t state;
static uint8_t bits;
static uint32_t frames;
static uint32_t samples;
static stream_task_callback_t callback;
void stream_task_init(stream_task_callback_t on_frame) { callback = on_frame; state = STREAM_STATE_IDLE; }
bool stream_task_start(uint8_t value)
{
    if (state != STREAM_STATE_IDLE || callback == NULL || !platform_stream_start(value)) return false;
    bits = value;
    frames = samples = 0;
    state = STREAM_STATE_RUN;
    return true;
}
void stream_task_stop(void) { platform_stream_stop(); state = STREAM_STATE_IDLE; }
stream_state_t stream_task_get_state(void) { return state; }
uint8_t stream_task_get_bits(void) { return bits; }
uint32_t stream_task_get_frames(void) { return frames; }
uint32_t stream_task_get_samples(void) { return samples; }
uint32_t stream_task_get_overruns(void) { return platform_stream_overruns(); }
void stream_task_handle(void)
{
    if (state != STREAM_STATE_RUN) return;
    uint32_t count;
    const uint8_t *data = platform_stream_next(&count);
    if (data == NULL) return;
    stream_header_t header = {STREAM_SYNC_WORD, (uint16_t)frames, (uint16_t)count};
    callback(&header, data, count * (bits == 8 ? 1 : 2));
    platform_stream_release();
    ++frames;
    samples += count;
}
