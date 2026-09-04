#ifndef NETCHESSZX_TEST_DIRECT_PARITY_H
#define NETCHESSZX_TEST_DIRECT_PARITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRECT_PARITY_ROLE_HOST 0u
#define DIRECT_PARITY_ROLE_GUEST 1u

#define DIRECT_PARITY_COLOR_WHITE 0u
#define DIRECT_PARITY_COLOR_BLACK 1u
#define DIRECT_PARITY_COLOR_UNKNOWN 0xffu

#define DIRECT_PARITY_LINK_NONE 0xffu

#define DIRECT_PARITY_IN_LINK_UP 1u
#define DIRECT_PARITY_IN_RX 2u
#define DIRECT_PARITY_IN_LINK_DOWN 3u
#define DIRECT_PARITY_IN_LOCAL 4u
#define DIRECT_PARITY_IN_DOMAIN 5u
#define DIRECT_PARITY_IN_TIMEOUT 6u
#define DIRECT_PARITY_IN_SEND_FAIL 7u
#define DIRECT_PARITY_IN_DECISION 8u
#define DIRECT_PARITY_IN_SEND_PENDING 9u
#define DIRECT_PARITY_IN_TX_GUARD_TIMEOUT 10u
#define DIRECT_PARITY_IN_TX_RESULT 11u

#define DIRECT_PARITY_REQUEST_START 1u
#define DIRECT_PARITY_REQUEST_MOVE 2u
#define DIRECT_PARITY_REQUEST_CHAT 3u
#define DIRECT_PARITY_REQUEST_RESET 4u
#define DIRECT_PARITY_REQUEST_DRAW 5u
#define DIRECT_PARITY_REQUEST_RESIGN 6u
#define DIRECT_PARITY_REQUEST_TAKEBACK 7u
#define DIRECT_PARITY_REQUEST_BYE 8u
#define DIRECT_PARITY_REQUEST_RESTORE 9u

#define DIRECT_PARITY_PHASE_READY 2u
#define DIRECT_PARITY_PHASE_ACTIVE 3u
#define DIRECT_PARITY_PHASE_OVER 4u

#define DIRECT_PARITY_DECISION_ACCEPT 1u
#define DIRECT_PARITY_DECISION_REJECT 2u

#define DIRECT_PARITY_OBS_SEND 1u
#define DIRECT_PARITY_OBS_READY 2u
#define DIRECT_PARITY_OBS_ENDED 3u
#define DIRECT_PARITY_OBS_SIDE 4u
#define DIRECT_PARITY_OBS_STARTED 5u
#define DIRECT_PARITY_OBS_GAME 6u
#define DIRECT_PARITY_OBS_CONTROL_RESULT 7u
#define DIRECT_PARITY_OBS_DECISION 8u
#define DIRECT_PARITY_OBS_CONTROL 9u
#define DIRECT_PARITY_OBS_CLOSE 10u
#define DIRECT_PARITY_OBS_CHAT 11u

#define DIRECT_PARITY_GAME_LOCAL_MOVE 1u
#define DIRECT_PARITY_GAME_REMOTE_MOVE 2u
#define DIRECT_PARITY_GAME_TAKEBACK 3u
#define DIRECT_PARITY_GAME_RESTORE 4u
#define DIRECT_PARITY_GAME_PLATFORM 5u

#define DIRECT_PARITY_CHAT_REMOTE 1u
#define DIRECT_PARITY_CHAT_LOCAL 2u

#define DIRECT_PARITY_RESULT_ACCEPTED 1u
#define DIRECT_PARITY_RESULT_REJECTED 2u
#define DIRECT_PARITY_RESULT_CANCELLED 3u
#define DIRECT_PARITY_RESULT_EXPIRED 4u

#define DIRECT_PARITY_TX_OK 1u
#define DIRECT_PARITY_TX_FAILED 2u

#define DIRECT_PARITY_PAYLOAD_CAPACITY 61u
#define DIRECT_PARITY_TRACE_CAPACITY 32u

typedef struct DirectParityStep {
    const uint8_t *payload;
    uint8_t type;
    uint8_t link_id;
    uint8_t length;
    uint16_t value;
    uint8_t request;
    /* Session phase for LOCAL; optional correlation id for DOMAIN,
       DECISION, and TX_RESULT (0 means the current pending id). */
    uint8_t phase;
} DirectParityStep;

typedef struct DirectParityObservation {
    uint8_t type;
    uint8_t link_id;
    uint8_t code;
    uint8_t length;
    uint16_t value;
    char payload[DIRECT_PARITY_PAYLOAD_CAPACITY];
} DirectParityObservation;

typedef struct DirectParityScenario {
    const char *id;
    const DirectParityStep *steps;
    const DirectParityObservation *expected;
    uint8_t step_count;
    uint8_t expected_count;
    uint8_t role;
    uint8_t host_color;
} DirectParityScenario;

typedef struct DirectParityTrace {
    DirectParityObservation observations[DIRECT_PARITY_TRACE_CAPACITY];
    uint8_t count;
} DirectParityTrace;

extern const DirectParityScenario direct_parity_scenarios[];
extern const uint8_t direct_parity_scenario_count;

uint8_t direct_reference_run(const DirectParityScenario *scenario,
                             DirectParityTrace *trace);
uint8_t direct_spectrum_run(const DirectParityScenario *scenario,
                            DirectParityTrace *trace);

#ifdef __cplusplus
}
#endif

#endif
