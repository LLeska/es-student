#include "pi-task/pi-task.h"
#include "systime/systime.h"

// About 585 cycles per term at 125 MHz: 64 terms take about 0.30 ms, below 0.5 ms.
static const uint32_t PI_TERMS_PER_CALL = 64;
static pi_state_t state;
static uint32_t total_terms;
static uint32_t pi_k;
static double sum;
static double sign;
static double value;
static uint64_t start_us;
static uint64_t elapsed_ms;

void pi_task_init(void)
{
    state = PI_NOT_STARTED;
}
bool pi_task_start(uint32_t terms)
{
    if (state == PI_RUNNING || terms == 0) return false;
    total_terms = terms;
    pi_k = 0;
    sum = 0.0;
    sign = 1.0;
    elapsed_ms = 0;
    start_us = systime_us();
    state = PI_RUNNING;
    return true;
}
void pi_task_handle(void)
{
    if (state != PI_RUNNING) return;
    // Subtract first: the last batch also works for UINT32_MAX terms.
    uint32_t remaining = total_terms - pi_k;
    uint32_t batch = remaining < PI_TERMS_PER_CALL ? remaining : PI_TERMS_PER_CALL;
    for (uint32_t n = 0; n < batch; ++n, ++pi_k) {
        sum += sign / (2.0 * pi_k + 1.0);
        sign = -sign;
    }
    if (pi_k == total_terms) {
        value = 4.0 * sum;
        elapsed_ms = (systime_us() - start_us) / 1000;
        state = PI_READY;
    }
}
void pi_task_get_result(pi_result_t *result)
{
    *result = (pi_result_t){.state = state, .value = value, .elapsed_ms = elapsed_ms};
}
