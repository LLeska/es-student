#include "profiling/profiling.h"
#include <stddef.h>
#if PROFILING_ENABLED
#include "pico/stdlib.h"

typedef struct {
    const char *name;
    uint32_t started_us;
    uint32_t count;
    uint32_t sum_us;
    uint32_t max_us;
    bool running;
} stopwatch_t;
static stopwatch_t stopwatches[PROFILING_STOPWATCH_COUNT];

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id < PROFILING_STOPWATCH_COUNT) stopwatches[id] = (stopwatch_t){.name = name};
}

void profiling_start(uint32_t id)
{
    if (id >= PROFILING_STOPWATCH_COUNT || stopwatches[id].name == NULL) return;
    stopwatches[id].started_us = time_us_32();
    stopwatches[id].running = true;
}

void profiling_stop(uint32_t id)
{
    if (id >= PROFILING_STOPWATCH_COUNT || !stopwatches[id].running) return;
    stopwatch_t *sw = &stopwatches[id];
    uint32_t elapsed_us = time_us_32() - sw->started_us;
    sw->running = false;
    ++sw->count;
    sw->sum_us += elapsed_us;
    if (elapsed_us > sw->max_us) sw->max_us = elapsed_us;
}

bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (id >= PROFILING_STOPWATCH_COUNT || result == NULL || stopwatches[id].name == NULL) return false;
    stopwatch_t *sw = &stopwatches[id];
    *result = (profiling_result_t){
        .name = sw->name, .count = sw->count,
        .mean_us = sw->count ? (double)sw->sum_us / sw->count : 0.0,
        .max_us = sw->max_us,
    };
    return true;
}

void profiling_reset(void)
{
    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; ++i) {
        stopwatches[i].count = 0;
        stopwatches[i].sum_us = 0;
        stopwatches[i].max_us = 0;
    }
}
#else
void profiling_stopwatch_init(uint32_t id, const char *name) { (void)id; (void)name; }
bool profiling_get(uint32_t id, profiling_result_t *result) { (void)id; (void)result; return false; }
void profiling_reset(void) {}
#endif
