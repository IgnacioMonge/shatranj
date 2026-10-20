#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/net.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/text.h"

#ifdef NETCHESSZX_SPECTRANEXT

#ifndef NETCHESSZX_HOST_TEST
#include "spxn.h"
#include "spxn_stream.h"
#include "spxmatch.h"
#include "spxinfo.h"
#endif

#define WAIT_LONG 2000u
#define DIRECT_KEY_CANCEL 0x8au
#define DIRECT_REARM_FRAMES 50u
#define DIRECT_LISTENER_ACTIVE 0xffu
#define DIRECT_LISTENER_RETRY 0xfeu
#define DIRECT_CTX_PTR_LO 0u
#define DIRECT_CTX_PTR_HI 1u
#define DIRECT_CTX_PAYLOAD_CAP 2u

extern char direct_rx_payload[];
extern char direct_rx_payload2[];
extern uint8_t active_link;
extern uint8_t direct_rx_count;
extern uint8_t direct_rx_head;
extern uint8_t direct_rx_payload_len;
extern uint8_t direct_rx_discard;
extern uint8_t direct_link_closed;
extern void net_wait_frame(void);

#ifdef NETCHESSZX_HOST_TEST
static char spxn_direct_spill[SPECTRUM_NET_PAYLOAD_MAX + 1u];
extern uint8_t spxn_direct_pending_cursor;
extern uint8_t spxn_direct_pending_length;
extern uint8_t spxn_direct_pending_scratch[];
char *direct_host_test_ptr;
#define SPXN_DIRECT_SPILL spxn_direct_spill
#define SPXN_DIRECT_PENDING_CURSOR spxn_direct_pending_cursor
#define SPXN_DIRECT_PENDING_LENGTH spxn_direct_pending_length
#define SPXN_DIRECT_SCRATCH spxn_direct_pending_scratch
#define direct_ctx_ptr(ctx) ((void)(ctx), direct_host_test_ptr)
#else
#define SPXN_DIRECT_SPILL ((char *)SPECTRUM_MQTT_RUNTIME_ASSETS_END)
#define SPXN_DIRECT_SPILL_END \
    (SPECTRUM_MQTT_RUNTIME_ASSETS_END + SPECTRUM_NET_PAYLOAD_MAX + 1u)
#if SPXN_DIRECT_SPILL_END != \
    NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_CURSOR_ADDR
#error "DIRECT pending cursor no longer follows spill"
#endif
#if NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_DATA_ADDR != \
    (NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_LENGTH_ADDR + 1u)
#error "DIRECT pending data no longer follows pending state"
#endif
/* The staged recv reuses the inactive MQTT stream, which the player identity
   follows (and net.c keeps the identity below the MQTT packet scratch). */
#if (NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_DATA_ADDR + \
     SPECTRUM_MQTT_PACKET_MAX) > \
    NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR
#error "DIRECT pending data overlaps the player identity"
#endif
#define SPXN_DIRECT_PENDING_CURSOR \
    (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_CURSOR_ADDR)
#define SPXN_DIRECT_PENDING_LENGTH \
    (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_LENGTH_ADDR)
#define SPXN_DIRECT_SCRATCH \
    ((uint8_t *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_DATA_ADDR)
#define direct_ctx_ptr(ctx) \
    ((char *)((uint16_t)(ctx)[DIRECT_CTX_PTR_LO] | \
              ((uint16_t)(ctx)[DIRECT_CTX_PTR_HI] << 8)))
#endif

static char *spxn_direct_slot_ovl(uint8_t slot)
{
    if (slot == 0u) {
        return direct_rx_payload;
    }
    if (slot == 1u) {
        return direct_rx_payload2;
    }
    return SPXN_DIRECT_SPILL;
}

static uint8_t spxn_direct_tail_ovl(void)
{
    uint8_t slot = (uint8_t)(direct_rx_head + direct_rx_count);

    if (slot >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
        slot = (uint8_t)(slot - SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT);
    }
    return slot;
}

static void spxn_direct_feed_ovl(uint8_t c)
{
    char *slot;

    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        if (!direct_rx_discard && direct_rx_payload_len != 0u &&
            direct_rx_count < SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
            slot = spxn_direct_slot_ovl(spxn_direct_tail_ovl());
            slot[direct_rx_payload_len] = '\0';
            direct_rx_payload_len = 0u;
            ++direct_rx_count;
        } else {
            direct_rx_payload_len = 0u;
        }
        direct_rx_discard = 0u;
        return;
    }
    if (c == 0u) {
        direct_rx_payload_len = 0u;
        direct_rx_discard = 1u;
        return;
    }
    if (direct_rx_discard ||
        direct_rx_count >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT ||
        direct_rx_payload_len >= (SPECTRUM_NET_PAYLOAD_MAX - 1u)) {
        direct_rx_discard = 1u;
        return;
    }
    slot = spxn_direct_slot_ovl(spxn_direct_tail_ovl());
    slot[direct_rx_payload_len++] = (char)c;
}

static uint8_t spxn_direct_drain_scratch_ovl(uint8_t cursor,
                                             uint8_t length)
{
    while (length != 0u &&
           direct_rx_count < SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
        spxn_direct_feed_ovl(SPXN_DIRECT_SCRATCH[cursor]);
        ++cursor;
        --length;
    }
    SPXN_DIRECT_PENDING_CURSOR = length != 0u ? cursor : 0u;
    SPXN_DIRECT_PENDING_LENGTH = length;
    return direct_rx_count != 0u;
}

/* One poll grants at most one recv. If both POLLIN and HUP are present, read
   first and leave the stream open until a later poll reports EOF; otherwise a
   short receive buffer would lose the final application bytes. */
static uint8_t spxn_direct_drain_ovl(void)
{
    int16_t flags;

    if (SPXN_DIRECT_PENDING_LENGTH != 0u) {
        return spxn_direct_drain_scratch_ovl(
            SPXN_DIRECT_PENDING_CURSOR, SPXN_DIRECT_PENDING_LENGTH);
    }

    flags = spxn_poll();
    if (flags < 0) {
        direct_link_closed = 1u;
        return 0u;
    }
    /* A matched stream keeps the connect notification raised; on a
       connected stream it is not news, so it must not starve this yield. */
    if ((flags & ~SPXN_POLLCON) == 0) {
        /* Match the ESP/UART read cadence. Besides keeping session timers in
           protocol frames, this yields to the cartridge/emulator I/O worker
           so a later TCP segment can make POLLIN observable. The yield also
           scans the keyboard. */
        net_wait_frame();
    }
    if ((flags & SPXN_POLLIN) != 0) {
        int16_t got = spxn_recv(SPXN_DIRECT_SCRATCH,
                                SPECTRUM_MQTT_PACKET_MAX);

        if (got < 0) {
            direct_link_closed = 1u;
            return 0u;
        }
        if (got == 0) {
            direct_link_closed = 1u;
            return 0u;
        }
        (void)spxn_direct_drain_scratch_ovl(0u, (uint8_t)got);
        /* POLLIN|POLLHUP is intentionally not closed here: the next poll
           drains any bytes that did not fit in this receive. */
        return direct_rx_count != 0u;
    }
    if ((flags & (SPXN_POLLHUP | SPXN_POLLNVAL)) != 0) {
        direct_link_closed = 1u;
    }
    return direct_rx_count != 0u;
}

static uint8_t spxn_direct_listen_ovl(void)
{
    spxn_close();
    SPXN_DIRECT_PENDING_CURSOR = 0u;
    SPXN_DIRECT_PENDING_LENGTH = 0u;
    active_link = DIRECT_LISTENER_RETRY;
    direct_link_closed = 1u;
    if (spxn_listen(netchesszx_direct_port) != SPXN_OK) {
        return 0u;
    }
    active_link = DIRECT_LISTENER_ACTIVE;
    direct_link_closed = 0u;
    return 1u;
}

/* ROM arguments live in the overlay scratch: the ROM cannot read this
   overlay's page, and a stack address (0xffxx) that GETHOSTBYNAME leaves in
   DE reaches SOCKET as D=0xff, where the firmware takes E as the socket's
   protocol selector. */
#ifdef NETCHESSZX_HOST_TEST
static char direct_match_host_arg[sizeof(NETCHESSZX_MATCH_HOST)];
static uint8_t direct_ip4_arg[4];
#define DIRECT_MATCH_HOST_ARG direct_match_host_arg
#define DIRECT_IP4_ARG direct_ip4_arg
#define DIRECT_MATCH_HOST_COPY strcpy
#else
#define DIRECT_MATCH_HOST_ARG \
    ((char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#define DIRECT_IP4_ARG \
    ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR + \
     sizeof(NETCHESSZX_MATCH_HOST))
#define DIRECT_MATCH_HOST_COPY spectrum_append_text
#endif

/* Two serial strings share the scratch once ROM CONNECT no longer needs the
   service name and address. */
#ifdef NETCHESSZX_HOST_TEST
static char direct_serial_arg[2u * SPXINFO_STRING_SIZE];
#define DIRECT_SERIAL_ARG direct_serial_arg
#else
#define DIRECT_SERIAL_ARG ((char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif
typedef char direct_serial_args_fit[
    (2u * SPXINFO_STRING_SIZE <= NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE)
        ? 1 : -1];

/* MATCH host lobby state survives between wait batches in low RAM. */
#ifdef NETCHESSZX_HOST_TEST
static uint8_t match_phase;
static uint8_t match_ticks;
static uint8_t match_socket;
static char match_saved_name[NETCHESSZX_LOWRAM_SPXN_MATCH_SAVED_NAME_SIZE];
#define MATCH_PHASE match_phase
#define MATCH_TICKS match_ticks
#define MATCH_SOCKET match_socket
#define MATCH_SAVED_NAME match_saved_name
#else
#define MATCH_PHASE (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_MATCH_PHASE_ADDR)
#define MATCH_TICKS (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_MATCH_TICKS_ADDR)
#define MATCH_SOCKET (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_MATCH_SOCKET_ADDR)
#define MATCH_SAVED_NAME \
    ((char *)NETCHESSZX_LOWRAM_SPXN_MATCH_SAVED_NAME_ADDR)
#endif
#define MATCH_PHASE_IDLE 0u
#define MATCH_PHASE_START 1u   /* MATCH start sent */
#define MATCH_PHASE_NAME 2u    /* SET_NAME sent */
#define MATCH_PHASE_PAUSE 3u   /* ticks count down to the next poll */
#define MATCH_PHASE_POLL 4u    /* MATCH poll sent */
#define MATCH_PHASE_PAIRED 5u  /* stage 3 returned; leave before re-opening */
#define MATCH_REPLY_FRAMES 250u
/* Saved-name byte 0 after a failed operation 21 read: cancel sends no
   SET_NAME. An empty saved name restores a blank one. */
#define MATCH_NAME_KEEP ((char)0xff)
/* The lobby name reuses the service-name argument: NAME + " (W)" + NUL. */
typedef char match_lobby_name_fits[
    (NETCHESS_PLAYER_NAME_SIZE + 4u <= sizeof(NETCHESSZX_MATCH_HOST) &&
     NETCHESS_PLAYER_NAME_SIZE + 3u <= SPXMATCH_NAME_MAX) ? 1 : -1];

/* Open a stream socket without ROM CONNECT and start MATCH on it. */
static uint8_t match_host_open(void)
{
    int16_t socket;

    spxn_close();
    MATCH_PHASE = MATCH_PHASE_IDLE;
    MATCH_TICKS = 0u;
    active_link = DIRECT_LISTENER_ACTIVE;
    (void)DIRECT_MATCH_HOST_COPY(DIRECT_MATCH_HOST_ARG,
                                 NETCHESSZX_MATCH_HOST);
    socket = spxmatch_open(DIRECT_MATCH_HOST_ARG, DIRECT_IP4_ARG);
    if (socket < 0) {
        return 0u;
    }
    MATCH_SOCKET = (uint8_t)socket;
    if (spxmatch_send(MATCH_SOCKET, SPXMATCH_CMD_MATCH, SPXMATCH_POLL, 0) !=
        SPXN_OK) {
        spxn_close();
        return 0u;
    }
    MATCH_PHASE = MATCH_PHASE_START;
    return 1u;
}

static void match_lobby_name(char *out)
{
    out = spectrum_append_text(out,
                               netchesszx_player_name[0] != '\0'
                                   ? (const char *)netchesszx_player_name
                                   : "HOST");
    (void)spectrum_append_text(out,
                               netchesszx_session_identity_host_color() !=
                                       NETCHESSZX_COLOR_WHITE
                                   ? " (B)" : " (W)");
}

/* Bounded wait for the outstanding mailbox reply (one command at a time):
   0 while the mailbox is still busy after MATCH_REPLY_FRAMES. */
static uint8_t match_reply_wait(void)
{
    uint8_t frames = MATCH_REPLY_FRAMES;

    while (spxmatch_status() == SPXMATCH_BUSY) {
        if (--frames == 0u) {
            return 0u;
        }
        net_wait_frame();
    }
    return 1u;
}

uint8_t direct_close_ovl(void)
{
    /* Established MATCH sockets close normally; lobby cancellation has its
       own CANCEL path. LEAVE would release the transport before ROM CLOSE. */
    spxn_close();
    MATCH_PHASE = MATCH_PHASE_IDLE;
    return 1u;
}

/* Keep the tunnel alive until the peer consumed BYE and closed, or the
   existing send budget expires. CLOSE can discard a queued MATCH write. */
#ifdef NETCHESSZX_HOST_TEST
uint8_t direct_close_wait_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t frames = ctx[SPECTRUM_OVL_CTX_DIRECT_CLOSE_WAIT];

    while (frames-- != 0u) {
        int16_t flags = spxn_poll();

        if (flags < 0 || (flags & (SPXN_POLLNVAL | SPXN_POLLHUP)) != 0) {
            break;
        }
        if ((flags & SPXN_POLLIN) != 0 &&
            spxn_recv(SPXN_DIRECT_SCRATCH, SPECTRUM_MQTT_PACKET_MAX) <= 0) {
            break;
        }
        net_wait_frame();
    }
    return direct_close_ovl();
}

#endif

static uint8_t match_send_wait(uint8_t command, uint8_t action,
                               const char *text)
{
    (void)spxmatch_send(MATCH_SOCKET, command, action, text);
    return match_reply_wait();
}

/* SET_NAME needs the open MATCH session, so it precedes cancel. A reply
   that never comes skips the remaining commands: none goes to a busy
   mailbox. */
static void match_host_cancel(void)
{
    if (MATCH_PHASE != MATCH_PHASE_IDLE && match_reply_wait() &&
        (MATCH_SAVED_NAME[0] == MATCH_NAME_KEEP ||
         match_send_wait(SPXMATCH_CMD_SET_NAME, 0u, MATCH_SAVED_NAME))) {
        (void)match_send_wait(SPXMATCH_CMD_MATCH, SPXMATCH_CANCEL, 0);
    }
    MATCH_PHASE = MATCH_PHASE_IDLE;
    spxn_close();
}

/* One mailbox step per frame. The stream is never read or written before
   stage 3: a blocking ROM call on an unpaired socket can hang. */
static uint8_t match_host_wait(void)
{
    uint16_t frames = WAIT_LONG;

    while (frames-- != 0u) {
        if (spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            match_host_cancel();
            return SPECTRUM_LINK_CANCELLED;
        }
        if (MATCH_TICKS != 0u) {
            --MATCH_TICKS;
        } else if (MATCH_PHASE == MATCH_PHASE_IDLE) {
            /* Register again; a refused socket retries at the re-arm pace. */
            if (!match_host_open()) {
                MATCH_TICKS = DIRECT_REARM_FRAMES;
            }
        } else if (MATCH_PHASE == MATCH_PHASE_PAUSE) {
            (void)spxmatch_send(MATCH_SOCKET, SPXMATCH_CMD_MATCH,
                                SPXMATCH_POLL, 0);
            MATCH_PHASE = MATCH_PHASE_POLL;
        } else {
            uint16_t stage = (uint16_t)spxmatch_status();

            /* The snapshot stage exists only in MATCH replies: a SET_NAME
               reply, whatever it holds, just leads to the next poll. */
            if (stage == SPXMATCH_BUSY) {
                /* Keep waiting. */
            } else if (MATCH_PHASE != MATCH_PHASE_NAME &&
                       stage >= SPXMATCH_STAGE_CONNECTED) {
                if (stage == SPXMATCH_STAGE_CONNECTED) {
                    /* The socket already is the driver's connected
                       stream. */
                    MATCH_PHASE = MATCH_PHASE_PAIRED;
                    active_link = 0u;
                    return 1u;
                }
                /* Cancelled, error or a controller failure: cancel through
                   MATCH and close, as the controller asks. The host shows
                   CONNECTION FAILED, then the next wait registers again
                   after the re-arm pause, keeping the saved name. */
                (void)match_send_wait(SPXMATCH_CMD_MATCH, SPXMATCH_CANCEL, 0);
                spxn_close();
                MATCH_PHASE = MATCH_PHASE_IDLE;
                MATCH_TICKS = DIRECT_REARM_FRAMES;
                return SPECTRUM_LINK_FAILED;
            } else if (MATCH_PHASE == MATCH_PHASE_START) {
                match_lobby_name(DIRECT_MATCH_HOST_ARG);
                (void)spxmatch_send(MATCH_SOCKET, SPXMATCH_CMD_SET_NAME, 0u,
                                    DIRECT_MATCH_HOST_ARG);
                MATCH_PHASE = MATCH_PHASE_NAME;
            } else {
                /* The controller rate-limits server refreshes: poll again
                   on the 50th frame, about once a second. */
                MATCH_TICKS = DIRECT_REARM_FRAMES - 1u;
                MATCH_PHASE = MATCH_PHASE_PAUSE;
            }
        }
        net_wait_frame();
    }
    return 0u;
}

/* Reads the cartridge name into the scratch, as cancel restores it: an
   unnamed cartridge reads as its serial (operation 21 id 0) and becomes
   blank, and a paired game's " (W)" or " (B)" is removed. Returns 0 without
   operation 21, 2 when it removed a mark, else 1. The scratch is read before
   match_host_open stages the service there. */
static uint8_t match_read_name(void)
{
    char *end = DIRECT_SERIAL_ARG;

    if (spxinfo_string(SPXINFO_STRING_MATCH_NAME, DIRECT_SERIAL_ARG,
                       SPXINFO_STRING_SIZE) != SPXN_OK) {
        return 0u;
    }
    while (*end != '\0') {
        ++end;
    }
    if (end - DIRECT_SERIAL_ARG >=
            NETCHESSZX_LOWRAM_SPXN_MATCH_SAVED_NAME_SIZE ||
        (spxinfo_string(SPXINFO_STRING_SERIAL,
                        DIRECT_SERIAL_ARG + SPXINFO_STRING_SIZE,
                        SPXINFO_STRING_SIZE) == SPXN_OK &&
         spectrum_streq(DIRECT_SERIAL_ARG,
                        DIRECT_SERIAL_ARG + SPXINFO_STRING_SIZE))) {
        DIRECT_SERIAL_ARG[0] = '\0';
    } else if (end - DIRECT_SERIAL_ARG > 3 && end[-4] == ' ' &&
               end[-3] == '(' && (end[-2] == 'W' || end[-2] == 'B') &&
               end[-1] == ')') {
        end[-4] = '\0';
        return 2u;
    }
    return 1u;
}

uint8_t direct_listen_ovl(void)
{
    /* MATCH hosts in the lobby without the firmware menu. The cartridge name
       is read once per listen, before SET_NAME replaces it, so that a
       re-registration keeps the name cancel restores. */
    if (netchesszx_link_match) {
        /* Close the last game's socket before reopening its lobby. */
        if (MATCH_PHASE == MATCH_PHASE_PAIRED) {
            (void)direct_close_ovl();
        }
        MATCH_SAVED_NAME[0] = MATCH_NAME_KEEP;
        if (match_read_name()) {
            (void)spectrum_append_text(MATCH_SAVED_NAME, DIRECT_SERIAL_ARG);
        }
        return match_host_open();
    }
    return spxn_direct_listen_ovl();
}

#include "match_picker.inc"

uint8_t direct_connect_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t *ip4be = DIRECT_IP4_ARG;

    /* JOIN, before the picker: a name a host session left marked
       would list this guest as a host. A short session restores it. */
    if (netchesszx_link_match) {
        if (ctx[SPECTRUM_OVL_CTX_DIRECT_UNMARK] == SPECTRUM_OVL_DIRECT_MATCH_PICK) {
            return match_guest_pick();
        }
        if (ctx[SPECTRUM_OVL_CTX_DIRECT_UNMARK] == SPECTRUM_OVL_DIRECT_MATCH_UNMARK) {
            if (match_read_name() == 2u) {
                (void)spectrum_append_text(MATCH_SAVED_NAME, DIRECT_SERIAL_ARG);
                if (match_host_open()) {
                    match_host_cancel();
                }
            }
            return 1u;
        }
        if (ctx[SPECTRUM_OVL_CTX_DIRECT_UNMARK] == SPECTRUM_OVL_DIRECT_MATCH_JOIN) {
            uint8_t result = match_guest_join();
            if (result != 1u) return result;
        }
    }
    active_link = DIRECT_LISTENER_ACTIVE;
    direct_link_closed = 0u;
    /* MATCH arrives already paired by the preceding picker call. */
    if (!netchesszx_link_match) {
        spxn_close();
        if (spxn_resolve(netchesszx_direct_host, ip4be) != SPXN_OK) {
            return 0u;
        }
        if (spxn_connect(ip4be, netchesszx_direct_port) != SPXN_OK) {
            spxn_close();
            return 0u;
        }
    } else if (
        /* The guest must be the initiator. Another guest that picked this one
           from its own menu leaves no host. Without operation 21 (first 1.2
           beta) a read fails and the check is skipped. */
        spxinfo_string(SPXINFO_STRING_SERIAL, DIRECT_SERIAL_ARG,
                       SPXINFO_STRING_SIZE) == SPXN_OK &&
        spxinfo_string(SPXINFO_STRING_MATCH_INITIATOR,
                       DIRECT_SERIAL_ARG + SPXINFO_STRING_SIZE,
                       SPXINFO_STRING_SIZE) == SPXN_OK &&
        !spectrum_streq(DIRECT_SERIAL_ARG,
                        DIRECT_SERIAL_ARG + SPXINFO_STRING_SIZE)) {
        (void)direct_close_ovl();
        return SPECTRUM_LINK_NOT_HOST;
    }
    if (netchesszx_link_match) {
        MATCH_PHASE = MATCH_PHASE_PAIRED;
    }
    active_link = 0u;
    return 1u;
}

uint8_t direct_wait_pc_connect_ovl(void)
{
    uint16_t frames = WAIT_LONG;
    uint8_t rearm = 0u;

    /* MATCH waits in the lobby: there is no listener to accept from. */
    if (netchesszx_link_match) {
        return match_host_wait();
    }
    if (active_link != DIRECT_LISTENER_RETRY) {
        active_link = DIRECT_LISTENER_ACTIVE;
    }
    while (frames-- != 0u) {
        int16_t flags;

        if (spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            spxn_close();
            active_link = DIRECT_LISTENER_RETRY;
            return SPECTRUM_LINK_CANCELLED;
        }
        if (active_link == DIRECT_LISTENER_RETRY) {
            /* A refused LISTEN (for example no free cartridge socket) is
               retried at a paced rate inside the batch. Returning at once made
               the caller repaint the wait notice in a tight loop, with the
               clock frozen and the cancel key never read. */
            if (rearm != 0u) {
                --rearm;
            } else if (!spxn_direct_listen_ovl()) {
                rearm = DIRECT_REARM_FRAMES;
            }
            net_wait_frame();
            continue;
        }
        flags = spxn_poll();
        if (flags < 0 || (flags & (SPXN_POLLHUP | SPXN_POLLNVAL)) != 0) {
            if (!spxn_direct_listen_ovl()) {
                rearm = DIRECT_REARM_FRAMES;
            }
            continue;
        }
        if ((flags & SPXN_POLLCON) != 0) {
            if (spxn_accept_single() == SPXN_OK) {
                active_link = 0u;
                direct_link_closed = 0u;
                return 1u;
            }
            if (!spxn_direct_listen_ovl()) {
                rearm = DIRECT_REARM_FRAMES;
            }
        }
        net_wait_frame();
    }
    /* The caller polls in bounded batches. An idle batch is not a listener
       failure: keep LISTEN (or its paced re-arm) alive so the next batch can
       accept a late peer. */
    return 0u;
}

uint8_t direct_read_payload_ovl(uint8_t *ctx) __z88dk_fastcall
{
    char *payload = direct_ctx_ptr(ctx);
    uint8_t payload_cap = ctx[DIRECT_CTX_PAYLOAD_CAP];
    uint8_t n = 0u;

    if (direct_rx_count == 0u && !direct_link_closed) {
        (void)spxn_direct_drain_ovl();
    }
    if (direct_rx_count != 0u) {
        char *source = spxn_direct_slot_ovl(direct_rx_head);

        if (payload_cap != 0u) {
            while (source[n] != '\0' && (uint8_t)(n + 1u) < payload_cap) {
                payload[n] = source[n];
                ++n;
            }
            payload[n] = '\0';
        }
        --direct_rx_count;
        if (++direct_rx_head == SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
            direct_rx_head = 0u;
        }
        return active_link;
    }
    if (direct_link_closed) {
        /* All bytes observed with POLLIN have already been delivered. */
        (void)direct_close_ovl();
        return 0xfeu;
    }
    return (uint8_t)SPECTRUM_NET_READ_TIMEOUT;
}

uint8_t direct_send_text_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *text = direct_ctx_ptr(ctx);
    const char newline = '\n';
    uint16_t len = 0u;

    while (text[len] != '\0' &&
           len < (SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP - 2u)) {
        ++len;
    }
    if (spxn_send_all(text, len, 150u) != SPXN_OK) {
        return 0u;
    }
    return (uint8_t)(spxn_send_all(&newline, 1u, 150u) == SPXN_OK);
}

#else

#define DIRECT_IPD_ACCEPT_BLOCK 1u
#define DIRECT_IPD_DISCARD_LINE 2u
#define WAIT_MED 500
#define WAIT_LONG 2000
#define WAIT_POLL 2
#define WAIT_DIRECT_PROMPT 50
#define WAIT_DIRECT_SEND_OK 150
#define DIRECT_SEND_GUARD_FRAMES 4u
#define DIRECT_UART_DRAIN_BUDGET 255u
#define DIRECT_KEY_CANCEL 0x8au
#define DIRECT_CIPSTART_PREFIX "AT+CIPSTART=\"TCP\",\""
#define DIRECT_CIPSTART_PREFIX_LEN 19u
#define DIRECT_CIPSTART_SUFFIX_LEN 2u
/* TCP keepalive (seconds): lets the guest ESP notice a dead peer and emit
   CLOSED instead of holding a half-open link forever. */
#define DIRECT_CIPSTART_KEEPALIVE ",60"
#define DIRECT_CIPSTART_KEEPALIVE_LEN 3u
#define DIRECT_PORT_MAX_LEN 5u
#define DIRECT_CIPSTART_MAX_LEN (DIRECT_CIPSTART_PREFIX_LEN + NETCHESSZX_DIRECT_HOST_MAX + DIRECT_CIPSTART_SUFFIX_LEN + DIRECT_PORT_MAX_LEN + DIRECT_CIPSTART_KEEPALIVE_LEN + 1u)
#if (SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP + DIRECT_CIPSTART_MAX_LEN) > SPECTRUM_MQTT_PACKET_MAX
#error "DIRECT command exceeds inactive MQTT packet scratch"
#endif

/* DIRECT lends the packet scratch prefix to the outgoing payload. Commands
   use its free tail, leaving both that payload and partial RX lines intact. */
#ifdef NETCHESSZX_HOST_TEST
static char direct_tx_scratch[SPECTRUM_MQTT_PACKET_MAX];
#else
#define direct_tx_scratch ((char *)SPECTRUM_MQTT_PACKET_SCRATCH)
#endif
#define DIRECT_COMMAND_BUF \
    (direct_tx_scratch + SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP)

#ifdef NETCHESSZX_NEXT
#ifdef NETCHESSZX_HOST_TEST
static uint8_t direct_recovery_state;
#define DIRECT_RECOVERY_STATE direct_recovery_state
#else
#define DIRECT_RECOVERY_STATE \
    (*(uint8_t *)(NETCHESSZX_LOWRAM_LAST_IP_ADDR + \
                  NETCHESSZX_LOWRAM_LAST_IP_SIZE - 1u))
#endif
#endif

extern char line_buf[];
extern char direct_rx_payload[];
extern char direct_rx_payload2[];
extern uint8_t direct_rx_link;
extern uint8_t direct_rx_link2;
extern uint8_t active_link;
extern uint8_t line_pos;
extern uint8_t direct_rx_count;
extern uint8_t direct_rx_head;
extern uint8_t direct_rx_payload_len;
extern uint16_t direct_ipd_remaining;
extern uint8_t direct_ipd_accept;
extern uint8_t direct_ipd_link;
extern uint8_t direct_link_closed;
extern uint8_t direct_peer_valid;
extern uint8_t direct_intruder_link;
#ifdef NETCHESSZX_HOST_TEST
static char direct_rx_spill[SPECTRUM_NET_PAYLOAD_MAX + 1u];
#define DIRECT_RX_SPILL direct_rx_spill
#else
#define DIRECT_RX_SPILL ((char *)SPECTRUM_MQTT_RUNTIME_ASSETS_END)
#endif
#if (SPECTRUM_MQTT_RUNTIME_ASSETS_END + SPECTRUM_NET_PAYLOAD_MAX + 1u) > SPECTRUM_MQTT_SCRATCH_BASE
#error "DIRECT spill exceeds inactive MQTT stream storage"
#endif
extern void reset_line_buf(void);
extern void net_wait_frame(void);
#ifdef NETCHESSZX_HOST_TEST
#define spectrum_net_at_cipserver_0 "AT+CIPSERVER=0"
#define spectrum_net_at_cipclose "AT+CIPCLOSE"
#define spectrum_net_at_cipmux_0 "AT+CIPMUX=0"
#else
extern const char spectrum_net_at_cipserver_0[];
extern const char spectrum_net_at_cipclose[];
extern const char spectrum_net_at_cipmux_0[];
#endif

#define direct_digit_value(c) ((uint8_t)((uint8_t)(c) - (uint8_t)'0'))
#define direct_is_digit(c) (direct_digit_value(c) <= 9u)
#define direct_is_link_digit(c) (direct_digit_value(c) <= 4u)
#define DIRECT_CTX_PTR_LO 0u
#define DIRECT_CTX_PTR_HI 1u
#define DIRECT_CTX_PAYLOAD_CAP 2u
#ifdef NETCHESSZX_HOST_TEST
/* Host regression tests can't pack a 64-bit pointer into the 2-byte ctx. */
char *direct_host_test_ptr;
#define direct_ctx_ptr(ctx) ((void)(ctx), direct_host_test_ptr)
#else
#define direct_ctx_ptr(ctx) \
    ((char *)((uint16_t)(ctx)[DIRECT_CTX_PTR_LO] | \
              ((uint16_t)(ctx)[DIRECT_CTX_PTR_HI] << 8)))
#endif

static char *direct_payload_slot_ovl(uint8_t slot) __z88dk_fastcall
{
    return slot == 0u ? direct_rx_payload
                      : (slot == 1u ? direct_rx_payload2
                                    : DIRECT_RX_SPILL);
}

static uint8_t direct_tail_slot_ovl(void)
{
    uint8_t slot = (uint8_t)(direct_rx_head + direct_rx_count);

    return slot < SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT
        ? slot : (uint8_t)(slot - SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT);
}

static char *direct_tail_payload_ovl(void)
{
    return direct_payload_slot_ovl(direct_tail_slot_ovl());
}

static uint8_t direct_head_link_ovl(void)
{
    return direct_rx_head == 0u ? direct_rx_link
                                : (direct_rx_head == 1u
                                       ? direct_rx_link2
                                       : (uint8_t)DIRECT_RX_SPILL[
                                             SPECTRUM_NET_PAYLOAD_MAX]);
}

static uint8_t direct_queue_payload_ovl(void)
{
    char *slot_payload;
    uint8_t slot;

    if (direct_rx_payload_len == 0u) {
        return 1u;
    }
    if (direct_rx_count >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
        direct_rx_payload_len = 0u;
        return 0u;
    }

    slot_payload = direct_tail_payload_ovl();
    slot_payload[direct_rx_payload_len] = '\0';
    direct_rx_payload_len = 0u;
    /* A peer that re-announces HELLO before it is answered leaves identical
       HELLOs queued, and its HELLO + MACH + MNAME answer then overflowed the
       three slots, dropping MNAME. A HELLO equal to the newest queued frame
       changes nothing the session does: it consumes duplicates, and a
       conflicting HELLO differs and still queues. It keeps no slot. */
    if (direct_rx_count != 0u && slot_payload[0] == 'H') {
        char *newest;

        --direct_rx_count;
        newest = direct_tail_payload_ovl();
        ++direct_rx_count;
        if (spectrum_streq(newest, slot_payload)) {
            return 1u;
        }
    }
    slot = direct_tail_slot_ovl();
    if (slot == 0u) {
        direct_rx_link = direct_ipd_link;
    } else if (slot == 1u) {
        direct_rx_link2 = direct_ipd_link;
    } else {
        DIRECT_RX_SPILL[SPECTRUM_NET_PAYLOAD_MAX] = (char)direct_ipd_link;
    }
    ++direct_rx_count;
    return 1u;
}

static uint8_t direct_parse_ipd_header_ovl(void)
{
    char *p = line_buf + 5;
    uint16_t len;
    uint8_t link = 0u;
    uint8_t have_link = 0u;

    for (;;) {
        len = 0u;
        if (!direct_is_digit((uint8_t)*p)) {
            return 0u;
        }
        do {
            len = (uint16_t)((len << 3) + (len << 1) +
                             direct_digit_value((uint8_t)*p));
            ++p;
        } while (direct_is_digit((uint8_t)*p));

        if (*p != ',') {
            break;
        }
        if (have_link) {
            return 0u;
        }
        link = (uint8_t)len;
        have_link = 1u;
        ++p;
    }

    if (*p != ':') {
        return 0u;
    }
    /* len == 0 wraps to UINT16_MAX, rejecting zero and len > 2048 at once. */
    if (link > 4u || (uint16_t)(len - 1u) >= 2048u) {
        return 0u;
    }

    direct_ipd_accept = (uint8_t)(DIRECT_IPD_ACCEPT_BLOCK |
        (direct_ipd_accept & DIRECT_IPD_DISCARD_LINE));
    if (active_link != 0xffu) {
        if (link != active_link) {
            /* Third party connected while the session is live: swallow the
               burst and flag it so the read path closes that link, keeping
               the active session untouched. */
            direct_ipd_accept &= DIRECT_IPD_DISCARD_LINE;
            direct_intruder_link = link;
        }
    }

    if ((direct_ipd_accept & DIRECT_IPD_ACCEPT_BLOCK) != 0u) {
        if (direct_ipd_link != link &&
            (direct_rx_payload_len != 0u ||
             (direct_ipd_accept & DIRECT_IPD_DISCARD_LINE) != 0u)) {
            direct_rx_payload_len = 0u;
            direct_ipd_accept = DIRECT_IPD_ACCEPT_BLOCK;
        }
        direct_ipd_link = link;
    } else if (direct_rx_payload_len == 0u &&
               (direct_ipd_accept & DIRECT_IPD_DISCARD_LINE) == 0u) {
        direct_ipd_link = link;
    }
    direct_ipd_remaining = len;
    reset_line_buf();
    return 1u;
}

static uint8_t direct_feed_payload_byte_ovl(uint8_t c)
{
    uint8_t queued = 0u;

    --direct_ipd_remaining;
    if ((direct_ipd_accept & DIRECT_IPD_ACCEPT_BLOCK) != 0u) {
        if (c == '\r') {
            /* ignore */
        } else if (c == '\n') {
            if ((direct_ipd_accept & DIRECT_IPD_DISCARD_LINE) != 0u) {
                direct_rx_payload_len = 0u;
                direct_ipd_accept &= DIRECT_IPD_ACCEPT_BLOCK;
            } else {
                queued = direct_queue_payload_ovl();
            }
        } else if (c == 0u) {
            direct_rx_payload_len = 0u;
            direct_ipd_accept |= DIRECT_IPD_DISCARD_LINE;
        } else if ((direct_ipd_accept & DIRECT_IPD_DISCARD_LINE) != 0u) {
            /* discard until this protocol line's LF, across +IPD blocks */
        } else if (direct_rx_count >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT ||
                   direct_rx_payload_len >= (SPECTRUM_NET_PAYLOAD_MAX - 1u)) {
            direct_rx_payload_len = 0u;
            direct_ipd_accept |= DIRECT_IPD_DISCARD_LINE;
        } else {
            direct_tail_payload_ovl()
                [direct_rx_payload_len++] = (char)c;
        }

    }

    if (direct_ipd_remaining == 0u) {
        /* Block exhausted without \n: keep the partial line so the next
           +IPD block continues it. Queuing it here forged messages when a
           line was split across TCP segments. */
        direct_ipd_accept &= DIRECT_IPD_DISCARD_LINE;
        reset_line_buf();
    }
    if (direct_link_closed) {
        return 3u;
    }
    return queued ? 2u : 0u;
}

static uint8_t direct_feed_uart_byte_ovl(uint8_t c)
{
    if (direct_ipd_remaining != 0u) {
        return direct_feed_payload_byte_ovl(c);
    }

    if (c == '>') {
        return 4u;
    }

    if (c == '\r') {
        return 0u;
    }

    if (c == '\n') {
        if (line_pos == SPECTRUM_NET_LINE_MAX) {
            reset_line_buf();
            return 0u;
        }
        if (line_pos != 0u) {
            line_buf[line_pos] = '\0';
            line_pos = 0u;
            if ((active_link == 0xffu &&
                 ((line_buf[0] == 'C' && line_buf[1] == 'L') ||
                  (line_buf[0] == 'U' && line_buf[1] == 'N'))) ||
                (direct_is_link_digit((uint8_t)line_buf[0]) &&
                 line_buf[1] == ',' &&
                 direct_digit_value((uint8_t)line_buf[0]) == active_link &&
                 line_buf[2] == 'C' && line_buf[3] == 'L')) {
                direct_link_closed = 1u;
                return 3u;
            }
            return 1u;
        }
        return 0u;
    }

    if (line_pos < (SPECTRUM_NET_LINE_MAX - 1u)) {
        line_buf[line_pos++] = (char)c;
        line_buf[line_pos] = '\0';
    } else {
        line_pos = SPECTRUM_NET_LINE_MAX;
    }

    if (c == ':' &&
        line_buf[0] == '+' && line_buf[1] == 'I' &&
        line_buf[2] == 'P' && line_buf[3] == 'D' &&
        line_buf[4] == ',') {
        if (direct_parse_ipd_header_ovl()) {
            return 0u;
        }
        reset_line_buf();
    }

    return 0u;
}

static uint8_t direct_drain_uart_ovl(void)
{
    uint8_t budget = DIRECT_UART_DRAIN_BUDGET;

    while (budget-- != 0u && spectrum_uart_ready()) {
        uint8_t rc = direct_feed_uart_byte_ovl(spectrum_uart_read());

        if (rc != 0u) {
            return rc;
        }
    }
    net_wait_frame();
    return 0u;
}

static uint8_t direct_wait_for_ok_ovl(uint16_t frames, uint8_t cancellable)
{
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl();

        if (cancellable && spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            return SPECTRUM_LINK_CANCELLED;
        }
        if (rc == 3u) {
            return 0u;
        }
        if (rc != 1u) {
            continue;
        }
        if (line_buf[0] == 'O' && line_buf[1] == 'K') {
            return 1u;
        }
        if (line_buf[0] == 'S' && line_buf[1] == 'E' &&
            line_buf[5] == 'O') {
            return 1u;
        }
        if (line_buf[0] == 'E' || line_buf[0] == 'F') {
            return 0u;
        }
        net_wait_frame();
    }
    return 0u;
}

static uint8_t direct_wait_for_prompt_ovl(uint16_t frames)
{
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl();

        if (rc == 4u) {
            return 1u;
        }
        if (rc == 3u) {
            return 0u;
        }
    }
    return 0u;
}

static uint8_t direct_send_command_ovl(void)
{
    uint8_t ok = spectrum_uart_send_string(DIRECT_COMMAND_BUF);

    if (!ok) {
        return 0u;
    }
    return spectrum_uart_send_crlf();
}

static uint8_t direct_tcp_connect_ovl(void)
{
    (void)spectrum_append_text(
        spectrum_append_u16(
            spectrum_append_text(
                spectrum_append_text(
                    spectrum_append_text(DIRECT_COMMAND_BUF, DIRECT_CIPSTART_PREFIX),
                    netchesszx_direct_host),
                "\","),
            netchesszx_direct_port),
        DIRECT_CIPSTART_KEEPALIVE);
    if (!direct_send_command_ovl()) {
        return 0u;
    }
    return direct_wait_for_ok_ovl(WAIT_LONG, 1u);
}

static uint8_t direct_prepare_link_ovl(void)
{
#ifdef NETCHESSZX_NEXT
    DIRECT_RECOVERY_STATE = 1u;
#endif
    if (!spectrum_net_ensure_command_mode()) {
        return 0u;
    }
    direct_link_closed = 0u;
    reset_line_buf();
    return 1u;
}

static void direct_reset_links_ovl(void)
{
    (void)spectrum_net_at_cmd(spectrum_net_at_cipserver_0, WAIT_MED);
    (void)spectrum_net_at_cmd("AT+CIPCLOSE=5", WAIT_MED);
}

static uint8_t direct_prepare_client_ovl(void)
{
    direct_reset_links_ovl();
    (void)spectrum_net_at_cmd(spectrum_net_at_cipclose, WAIT_MED);
    return spectrum_net_at_cmd(spectrum_net_at_cipmux_0, WAIT_MED);
}

uint8_t direct_listen_ovl(void)
{
    char *cmd;

    active_link = 0u;

    if (!direct_prepare_link_ovl()) {
        return 0u;
    }

    /* Re-listen after a lost session: stop the accept loop and close every
       stale/half-open server link first, or the ESP hits its 5-link cap and
       rejects all reconnect attempts. Both commands may reply ERROR on a
       fresh start (no server, no links) -- ignore. */
    direct_reset_links_ovl();

    if (!spectrum_net_at_cmd("AT+CIPMUX=1", WAIT_MED)) {
        return 0u;
    }

    cmd = spectrum_append_text(DIRECT_COMMAND_BUF, spectrum_net_at_cipserver_0);
    cmd[-1] = '1';
    *cmd++ = ',';
    (void)spectrum_append_u16(cmd, netchesszx_direct_port);
    if (!direct_send_command_ovl()) {
        return 0u;
    }
    if (!direct_wait_for_ok_ovl(WAIT_MED, 0u)) {
        return 0u;
    }

    return 1u;
}

uint8_t direct_connect_ovl(void)
{
    active_link = 0xffu;
    if (!direct_prepare_link_ovl()) {
        return 0u;
    }
    if (!direct_prepare_client_ovl()) {
        return 0u;
    }
    return direct_tcp_connect_ovl();
}

uint8_t direct_wait_pc_connect_ovl(void)
{
    uint16_t frames = WAIT_LONG;

    if (direct_rx_count != 0u) {
        goto payload_queued;
    }
    active_link = 0xffu;
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl();

        if (spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            return SPECTRUM_LINK_CANCELLED;
        }
        if (rc == 2u) {
            goto payload_queued;
        }
        if (rc == 3u) {
            active_link = 0u;
            return 0u;
        }
        if (rc == 1u && direct_is_link_digit(line_buf[0]) &&
            line_buf[1] == ',' && line_buf[2] == 'C' &&
            line_buf[3] == 'O') {
            active_link = direct_digit_value(line_buf[0]);
            return 1u;
        }
        net_wait_frame();
    }

    return 0u;

payload_queued:
    active_link = direct_head_link_ovl();
    return 1u;
}

static uint8_t direct_send_payload_ovl(const char *text, uint8_t link);

/* Close a third party without touching active_link. In CIPMUX=1, ESP-AT
   reports link closure as "<id>,CLOSED"; generic command ERROR belongs only
   to the rejected link operation. */
static void direct_reject_intruder_ovl(void)
{
    uint8_t link = direct_intruder_link;
    char *cmd;

    direct_intruder_link = 0xffu;
    if (direct_peer_valid) {
        (void)direct_send_payload_ovl("BUSY", link);
    }
    cmd = spectrum_append_text(DIRECT_COMMAND_BUF, spectrum_net_at_cipclose);
    *cmd++ = '=';
    (void)spectrum_append_u16(cmd, link);
    (void)direct_send_command_ovl();
    /* CIPCLOSE answers in ms. Do not block the active peer for the full send
       timeout; any late id-specific response is safe for the next poll. */
    (void)direct_wait_for_ok_ovl(DIRECT_SEND_GUARD_FRAMES, 0u);
}

uint8_t direct_read_payload_ovl(uint8_t *ctx) __z88dk_fastcall
{
#ifdef NETCHESSZX_SDCC_IY
    /* The dispatcher enters under DI. The sdcc_iy build keeps the shared
       live-interrupt IY contract while UART drain and DIRECT parsing run. */
#asm
    ei
#endasm
#endif
    char *payload = direct_ctx_ptr(ctx);
    uint8_t payload_cap = ctx[DIRECT_CTX_PAYLOAD_CAP];
    uint8_t n;

    if (direct_intruder_link != 0xffu && direct_ipd_remaining == 0u) {
        direct_reject_intruder_ovl();
    }
    if (direct_rx_count == 0u && !direct_link_closed) {
        if (direct_drain_uart_ovl() == 0u) {
            (void)direct_drain_uart_ovl();
        }
    }
    if (direct_rx_count != 0u) {
        char *slot_payload = direct_payload_slot_ovl(direct_rx_head);

        n = 0u;
        if (payload_cap != 0u) {
            while (slot_payload[n] != '\0' &&
                   n < (uint8_t)(payload_cap - 1u)) {
                payload[n] = slot_payload[n];
                ++n;
            }
            payload[n] = '\0';
        }
        n = direct_head_link_ovl();
        --direct_rx_count;
        /* Only advance head: the accumulation slot is (head+count)%3, so
           resetting head on empty (or zeroing direct_rx_payload_len) here
           would clobber a partial line still gathering in the other slot. */
        if (++direct_rx_head >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
            direct_rx_head = 0u;
        }
        return n;
    }
    if (direct_link_closed) {
        return 0xfeu;
    }
    return (uint8_t)SPECTRUM_NET_READ_TIMEOUT;
}

static uint8_t direct_send_payload_ovl(const char *text, uint8_t link)
{
    char *cmd;
    uint8_t payload_len;
    uint8_t len;

    payload_len = 0u;
    while (text[payload_len] != '\0' &&
           payload_len < (SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP - 2u)) {
        ++payload_len;
    }
    len = (uint8_t)(payload_len + 1u);

    spectrum_net_guard_wait(DIRECT_SEND_GUARD_FRAMES);

    cmd = spectrum_append_text(DIRECT_COMMAND_BUF, "AT+CIPSEND=");
    if (link != 0xffu) {
        cmd = spectrum_append_text(spectrum_append_u16(cmd, link), ",");
    }
    (void)spectrum_append_u16(cmd, len);
    if (!direct_send_command_ovl()) {
        return 0u;
    }

    if (!direct_wait_for_prompt_ovl(WAIT_DIRECT_PROMPT)) {
        return 0u;
    }

    if (!spectrum_uart_send_bytes((const uint8_t *)text, payload_len) ||
        !spectrum_uart_send_bytes((const uint8_t *)"\n", 1u)) {
        return 0u;
    }
    /* TCP payload queued before CLOSED is still valid application data.
       Let the poll layer deliver it before reporting EOF on the next read. */
    return (uint8_t)(direct_wait_for_ok_ovl(WAIT_DIRECT_SEND_OK, 0u) +
                     (direct_link_closed && direct_rx_count));
}

uint8_t direct_send_text_ovl(uint8_t *ctx) __z88dk_fastcall
{
    return direct_send_payload_ovl(direct_ctx_ptr(ctx), active_link);
}

#endif
