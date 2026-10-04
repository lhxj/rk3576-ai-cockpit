/* SPDX-License-Identifier: GPL-2.0 */
/* Test-only original HELLO/PING/PONG. Caller serializes all access. */
#ifndef COCKPIT_AMP_HEALTH_STATE_H
#define COCKPIT_AMP_HEALTH_STATE_H

#define AMP_HEALTH_INTERVAL_MS 1000ULL
#define AMP_HEALTH_REPLY_MS 3000ULL
#define AMP_HEALTH_WINDOW_MS 720000ULL

enum amp_health_phase {
    AMP_HEALTH_WAIT_ACK, AMP_HEALTH_READY, AMP_HEALTH_WAIT_PONG,
    AMP_HEALTH_FAILED, AMP_HEALTH_DONE
};

struct amp_health_state {
    enum amp_health_phase phase;
    unsigned long long started_ms, observed_ms, deadline_ms, next_ms;
    unsigned long long sent_ms, last_pong_ms, rtt_ms;
    unsigned int hello_ack, ping_count, pong_count, timeout_count, error_count;
};

static inline void amp_health_fail(struct amp_health_state *state)
{
    state->error_count++;
    state->phase = AMP_HEALTH_FAILED;
}

static inline int amp_health_observe(struct amp_health_state *state,
                                    unsigned long long now)
{
    if (state->phase == AMP_HEALTH_FAILED || state->phase == AMP_HEALTH_DONE)
        return 0;
    if (now < state->observed_ms) {
        amp_health_fail(state);
        return 0;
    }
    state->observed_ms = now;
    if ((state->phase == AMP_HEALTH_WAIT_ACK || state->phase == AMP_HEALTH_WAIT_PONG) &&
        now >= state->deadline_ms) {
        state->timeout_count++;
        state->phase = AMP_HEALTH_FAILED;
        return 0;
    }
    if (now - state->started_ms >= AMP_HEALTH_WINDOW_MS) {
        if (state->phase == AMP_HEALTH_READY)
            state->phase = AMP_HEALTH_DONE;
        else
            amp_health_fail(state);
        return 0;
    }
    return 1;
}

/* Returns 1 exactly when one PING must be sent; marks it in-flight first. */
static inline int amp_health_poll(struct amp_health_state *state,
                                 unsigned long long now)
{
    if (!amp_health_observe(state, now) || state->phase != AMP_HEALTH_READY ||
        now < state->next_ms)
        return 0;
    state->phase = AMP_HEALTH_WAIT_PONG;
    state->ping_count++;
    state->sent_ms = now;
    state->deadline_ms = now + AMP_HEALTH_REPLY_MS;
    return 1;
}

/* kind: 1 HELLO_ACK, 2 PONG, 0 malformed. Late/duplicate replies fail closed. */
static inline void amp_health_receive(struct amp_health_state *state,
                                     int kind, unsigned long long now)
{
    if (!amp_health_observe(state, now))
        return;
    if (kind == 1 && state->phase == AMP_HEALTH_WAIT_ACK) {
        state->hello_ack++;
        state->phase = AMP_HEALTH_READY;
        state->next_ms = now;
    } else if (kind == 2 && state->phase == AMP_HEALTH_WAIT_PONG) {
        state->pong_count++;
        state->last_pong_ms = now;
        state->rtt_ms = now - state->sent_ms;
        state->phase = AMP_HEALTH_READY;
        state->next_ms = now + AMP_HEALTH_INTERVAL_MS;
    } else {
        amp_health_fail(state);
    }
}

#endif
