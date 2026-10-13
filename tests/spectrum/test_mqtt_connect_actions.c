/* Execute the production coordinator/builders; only byte transport is stubbed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "spectrum/session/mqtt_wire.h"

static const char mqtt_meta_suffix[] = "meta";
static unsigned failure_stage, stage, aborted, publishes;
static char sent[3][10];
static char topics[3][8];
static uint8_t retained[3];
void netchesszx_host_mqtt_observe_route(uint8_t route) { (void)route; }

#include "../../src/spectrum/session/mqtt_connect_actions_ovl.c"
#include "../../src/spectrum/session/mqtt_wire.c"
#include "../../src/spectrum/session/identity.c"
#include "text_asm_host.c"

static uint8_t mqtt_connect_transport_start_ovl(void) { return 1u; }
static uint8_t mqtt_connect_transport_subscribe_side_ovl(void)
{
    return ++stage != failure_stage;
}
static uint8_t mqtt_connect_transport_abort_ovl(void)
{
    ++aborted;
    return 0u;
}
static uint8_t mqtt_connect_transport_publish_ovl(const char *suffix,
    const char *payload, uint8_t retain)
{
    assert(publishes < 3u && strlen(payload) < sizeof sent[0]);
    assert(strlen(suffix) < sizeof topics[0]);
    strcpy(sent[publishes], payload);
    strcpy(topics[publishes], suffix);
    retained[publishes++] = retain;
    return ++stage != failure_stage;
}

int main(void)
{
    unsigned role, fail;
    for (role = 0u; role < 2u; ++role) {
        netchesszx_session_configure((uint8_t)role,
            NETCHESSZX_TRANSPORT_MQTT, NETCHESSZX_COLOR_WHITE);
        netchesszx_mqtt_session_id = 65535u;
        for (fail = 0u; fail <= 3u; ++fail) {
            failure_stage = fail;
            stage = aborted = publishes = 0u;
            assert(mqtt_activate_side_ovl() == (fail == 0u));
            assert(aborted == (fail != 0u));
            assert(publishes == (fail == 1u ? 0u : fail == 2u ? 1u :
                                fail == 3u ? 3u : 2u));
            if (publishes) {
                assert(strcmp(sent[0], role ? "O B 65535" : "O W 65535") == 0);
                assert(strcmp(topics[0], role ? "pres_b" : "pres_w") == 0);
                assert(retained[0] == 1u);
            }
            if (publishes >= 2u) {
                assert(strcmp(sent[1], role ? "J 65535" : "H W 65535") == 0);
                assert(strcmp(topics[1], "meta") == 0);
                assert(retained[1] == !role);
            }
            if (fail == 3u) {
                assert(strcmp(sent[2], role ? "F B 65535" : "F W 65535") == 0);
                assert(strcmp(topics[2], topics[0]) == 0 && retained[2] == 1u);
            }
        }
    }
    puts("MQTT activation failure ordering and exact payloads ok");
    return 0;
}
