/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "amp_health_state.h"

static struct amp_health_state initial(void)
{
    struct amp_health_state state;
    memset(&state, 0, sizeof(state));
    state.started_ms = 100;
    state.observed_ms = 100;
    state.deadline_ms = 3100;
    state.phase = AMP_HEALTH_WAIT_ACK;
    return state;
}

int main(void)
{
    struct amp_health_state state = initial();
    unsigned int count;
    assert(!amp_health_poll(&state, 110));
    amp_health_receive(&state, 1, 120);
    assert(state.hello_ack == 1 && state.phase == AMP_HEALTH_READY);
    assert(amp_health_poll(&state, 120));
    assert(!amp_health_poll(&state, 121));
    amp_health_receive(&state, 2, 130);
    assert(state.ping_count == 1 && state.pong_count == 1 && state.rtt_ms == 10);
    assert(!amp_health_poll(&state, 1129));
    assert(amp_health_poll(&state, 1130));
    amp_health_receive(&state, 2, 1140);
    assert(state.ping_count == 2 && state.pong_count == 2);
    amp_health_receive(&state, 2, 1141); /* Duplicate is a hard error. */
    assert(state.phase == AMP_HEALTH_FAILED && state.error_count == 1);
    assert(!amp_health_poll(&state, 5000));

    state = initial();
    amp_health_receive(&state, 2, 110); /* PONG before HELLO_ACK. */
    assert(state.phase == AMP_HEALTH_FAILED && state.pong_count == 0);
    state = initial();
    amp_health_receive(&state, 0, 110); /* Wrong source/length/text. */
    assert(state.phase == AMP_HEALTH_FAILED);
    state = initial();
    assert(!amp_health_poll(&state, 3100));
    assert(state.timeout_count == 1 && state.phase == AMP_HEALTH_FAILED);
    amp_health_receive(&state, 1, 3101); /* Late ACK never opens a new request. */
    assert(state.hello_ack == 0 && !amp_health_poll(&state, 3200));

    state = initial();
    amp_health_receive(&state, 1, 120);
    assert(amp_health_poll(&state, 130));
    amp_health_receive(&state, 2, 3130); /* Deadline equality is expired. */
    assert(state.timeout_count == 1 && state.pong_count == 0);
    amp_health_receive(&state, 2, 3131);
    assert(state.pong_count == 0 && state.ping_count == 1);
    state = initial();
    amp_health_receive(&state, 1, 120);
    amp_health_receive(&state, 1, 121); /* Duplicate ACK. */
    assert(state.phase == AMP_HEALTH_FAILED);
    state = initial();
    assert(!amp_health_poll(&state, 99));
    assert(state.phase == AMP_HEALTH_FAILED && state.error_count == 1);
    state = initial();
    amp_health_receive(&state, 1, 120);
    amp_health_fail(&state); /* Send failure never retries. */
    assert(!amp_health_poll(&state, 200));

    state = initial();
    amp_health_receive(&state, 1, 120);
    for (count = 0; count < 600; ++count) {
        unsigned long long now = 120 + count * 1010ULL;
        assert(amp_health_poll(&state, now));
        assert(!amp_health_poll(&state, now + 1));
        amp_health_receive(&state, 2, now + 10);
        assert(state.phase == AMP_HEALTH_READY);
    }
    assert(state.ping_count == 600 && state.pong_count == 600);
    assert(state.timeout_count == 0 && state.error_count == 0);
    assert(!amp_health_poll(&state, 100 + AMP_HEALTH_WINDOW_MS));
    assert(state.phase == AMP_HEALTH_DONE);
    assert(!amp_health_poll(&state, 100 + AMP_HEALTH_WINDOW_MS + 1));
    puts("AMP_HEALTH_STATE_HOST_PASS: real shared state-machine, 600 simulated round trips");
    return 0;
}
