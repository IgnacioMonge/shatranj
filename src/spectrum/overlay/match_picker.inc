/* DIRECT-private guest UI. The snapshot reuses ROM argument scratch; the
   selected identity lives on the stack across replies, never in Page B. */
#include "spectrum/platform/input.h"
#include <stddef.h>

void match_picker_frame(void);
void match_picker_clear(void);
void match_picker_row(const char *line) __z88dk_fastcall;
void match_picker_finish(void);
void match_picker_cursor(uint8_t row) __z88dk_fastcall;

#ifdef NETCHESSZX_HOST_TEST
static struct spxmatch_snapshot match_snapshot;
#define MATCH_SNAPSHOT (&match_snapshot)
#else
#define MATCH_SNAPSHOT ((struct spxmatch_snapshot *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif
typedef char match_snapshot_fits[
    sizeof(struct spxmatch_snapshot) <= NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE ? 1 : -1];
typedef char match_host_name_fits[
    sizeof(MATCH_SNAPSHOT->name) <= 25u ? 1 : -1];

#define MATCH_NO_SELECTION 255u
#define MATCH_PICK_CANCEL (-127)
#define MATCH_PICK_CHANGED (-126)
/* Guest cancellation keeps the cartridge name. Its unused saved-name tail
   holds the choice across GUI overlay calls; byte 0 stays MATCH_NAME_KEEP. */
#define MATCH_PICK_SLOT MATCH_SAVED_NAME[1]
#define MATCH_PICK_REV_LO MATCH_SAVED_NAME[2]
#define MATCH_PICK_REV_HI MATCH_SAVED_NAME[3]
#define MATCH_PICK_PENDING (((uint8_t *)MATCH_SAVED_NAME)[4])

/* BUSY stays 16-bit; only completed replies and driver errors are narrowed. */
static int8_t match_picker_step(void)
{
    int16_t result = spxmatch_read(MATCH_SNAPSHOT);

    if (result == SPXMATCH_BUSY)
        return --MATCH_PICK_PENDING ? 0 : SPXN_EROM;
    MATCH_PICK_PENDING = 0u;
    return result;
}

static int8_t match_picker_reply(void)
{
    int8_t result;

    /* The first check is immediate; subsequent checks follow waited frames. */
    MATCH_PICK_PENDING = MATCH_REPLY_FRAMES + 1u;
    for (;;) {
        result = match_picker_step();
        if (!MATCH_PICK_PENDING) return result;
        if (spectrum_input_pending_event == DIRECT_KEY_CANCEL) {
            (void)spectrum_key_poll();
            MATCH_PICK_PENDING = 0u;
            return MATCH_PICK_CANCEL;
        }
        net_wait_frame();
    }
}

static int8_t match_picker_poll(void)
{
    int8_t stage = spxmatch_send(MATCH_SOCKET, SPXMATCH_CMD_MATCH, SPXMATCH_POLL, 0);
    if (stage == SPXN_OK) MATCH_PICK_PENDING = MATCH_REPLY_FRAMES;
    return stage;
}

static int8_t match_picker_peer(uint8_t slot, uint16_t revision)
{
    int8_t result = spxmatch_select(MATCH_SOCKET, SPXMATCH_CMD_GET_PEER,
                                     slot, revision);
    if (result == SPXN_OK) result = match_picker_reply();
    if (result >= 0 && MATCH_SNAPSHOT->revision != revision) return MATCH_PICK_CHANGED;
    return result;
}

/* The ASM formatter shares the SDK snapshot layout, checked at compile time. */
typedef char match_snapshot_layout[
    offsetof(struct spxmatch_snapshot, device) == 4u &&
    offsetof(struct spxmatch_snapshot, name) == 69u &&
    offsetof(struct spxmatch_snapshot, requires_code) == 93u ? 1 : -1];
#ifndef NETCHESSZX_HOST_TEST
uint8_t match_picker_label(char *line, const struct spxmatch_snapshot *snapshot) __z88dk_callee;
#else
/* A row descriptor is row index followed by 27 characters and NUL. */
uint8_t match_picker_label(char *line, const struct spxmatch_snapshot *snapshot)
{
    const char *name = snapshot->name;
    uint8_t n = 0u;
    const char *role;
    uint8_t i;

    while (name[n]) ++n;
    if (!snapshot->device[0] || snapshot->requires_code || n < 5u)
        return 0u;
    n -= 4u;
    role = name + n;
    if (role[0] != ' ' || role[1] != '(' || role[3] != ')' ||
        (role[2] != 'W' && role[2] != 'B')) return 0u;
    /* The SDK bounds names to 23 bytes, so the host part is at most 19. */
    for (i = 0u; i < n; ++i) {
        char c = name[i];
        line[i + 1u] = c >= 32 && c <= 126 ? c : '?';
    }
    for (; i < 22u; ++i) line[i + 1u] = ' ';
    (void)spectrum_append_text(line + 23, role[2] == 'W' ? "WHITE" : "BLACK");
    return 1u;
}

#endif

static uint8_t match_guest_pick(void)
{
    char identity[65];
    /* Keep full identities locally: navigation never waits for GET_PEER. */
    char storage[SPXMATCH_MAX_PEERS * 65u];
    char *devices[SPXMATCH_MAX_PEERS];
    char line[29];
    uint8_t slots[SPXMATCH_MAX_PEERS];
    uint8_t count = 0u;
    uint8_t selected = MATCH_NO_SELECTION;
    uint8_t first = 1u;
    uint8_t refresh = 1u;
    uint8_t ticks = 0u;
    uint16_t revision = 0u;
    int8_t stage;

    identity[0] = '\0';
    MATCH_SAVED_NAME[0] = MATCH_NAME_KEEP;
    if (!match_host_open()) return 0u;
    match_picker_frame();
    stage = match_picker_reply();
    for (;;) {
        uint8_t key;

        if (MATCH_PICK_PENDING)
            /* Keep queued keys until a changed list has been rebuilt. */
            stage = refresh ? match_picker_reply() : match_picker_step();
        if (stage == MATCH_PICK_CHANGED) {
            refresh = 1u;
            stage = match_picker_poll();
            continue;
        }
        if (stage == MATCH_PICK_CANCEL) break;
        /* Only stages 0..3 continue; negative driver errors also exit. */
        if ((uint8_t)stage >= SPXMATCH_STAGE_CANCELLED) {
            match_host_cancel();
            return 0u;
        }
        if (stage == SPXMATCH_STAGE_CONNECTED) {
            (void)direct_close_ovl();
            return SPECTRUM_LINK_NOT_HOST;
        }
        if (!MATCH_PICK_PENDING && (uint8_t)stage < 2u &&
            (refresh || revision != MATCH_SNAPSHOT->revision)) {
            uint8_t limit = MATCH_SNAPSHOT->count;
            uint8_t slot;
            char *device = storage;

            if (selected != MATCH_NO_SELECTION)
                (void)spectrum_append_text(identity, devices[selected]);
            revision = MATCH_SNAPSHOT->revision;
            selected = MATCH_NO_SELECTION;
            count = 0u;
            match_picker_clear();
            for (slot = 0u; slot < limit; ++slot) {
                stage = match_picker_peer(slot, revision);
                if (stage < 0) break;
                if (!match_picker_label(line, MATCH_SNAPSHOT)) continue;
                slots[count] = slot;
                devices[count] = device;
                device = spectrum_append_text(device, MATCH_SNAPSHOT->device) + 1;
                line[0] = (char)count;
                match_picker_row(line);
                if (first || spectrum_streq(identity, MATCH_SNAPSHOT->device)) {
                    selected = count;
                    first = 0u;
                }
                ++count;
            }
            if (stage < 0) continue;
            if (count == 0u) {
                first = 1u;
                line[0] = 0;
                (void)spectrum_append_text(line + 1, "NO HOSTS AVAILABLE");
                match_picker_row(line);
            }
            match_picker_finish();
            if (selected == MATCH_NO_SELECTION) identity[0] = '\0';
            else match_picker_cursor(selected);
            refresh = 0u;
            ticks = DIRECT_REARM_FRAMES;
        }
        key = spectrum_key_poll();
        if (key == DIRECT_KEY_CANCEL) break;
        if (!refresh && (uint8_t)stage < 2u && count != 0u) {
            if (key == '7' || key == 'q') key = 0x81u;
            if (key == '6' || key == 'a') key = 0x82u;
            if (key == 0x81u || key == 0x82u) {
                uint8_t next = (uint8_t)(selected + (key == 0x81u ? -1 : 1));
                if (selected == MATCH_NO_SELECTION) next = 0u;
                if (next < count) {
                    match_picker_cursor(selected);
                    selected = next;
                    match_picker_cursor(selected);
                }
            } else if ((key == 13u || key == 32u) && selected != MATCH_NO_SELECTION) {
                MATCH_PICK_SLOT = (char)slots[selected];
                MATCH_PICK_REV_LO = (char)revision;
                MATCH_PICK_REV_HI = (char)(revision >> 8);
                spectrum_input_suppress_until_release(key);
                return 1u;
            }
        }
        net_wait_frame();
        if (ticks != 0u) --ticks;
        else if (!MATCH_PICK_PENDING) {
            stage = match_picker_poll();
            ticks = DIRECT_REARM_FRAMES - 1u;
        }
    }
    match_host_cancel();
    return SPECTRUM_LINK_CANCELLED;
}

static uint8_t match_guest_join(void)
{
    int8_t stage;

    if (MATCH_PICK_PENDING) {
        stage = match_picker_reply();
        if (stage == SPXMATCH_STAGE_CONNECTED) {
            (void)direct_close_ovl();
            return SPECTRUM_LINK_NOT_HOST;
        }
        if ((uint8_t)stage >= SPXMATCH_STAGE_CANCELLED) goto failed;
    }
    stage = spxmatch_select(MATCH_SOCKET, SPXMATCH_CMD_CONNECT,
        (uint8_t)MATCH_PICK_SLOT,
        (uint16_t)(uint8_t)MATCH_PICK_REV_LO |
        ((uint16_t)(uint8_t)MATCH_PICK_REV_HI << 8));

    if (stage == SPXN_OK) stage = match_picker_reply();
    if (stage == 2) {
        MATCH_PHASE = MATCH_PHASE_POLL;
        stage = match_host_wait();
        if (stage != 0) return (uint8_t)stage;
    }
    if (stage == SPXMATCH_STAGE_CONNECTED) return 1u;
failed:
    match_host_cancel();
    return stage == MATCH_PICK_CANCEL ? SPECTRUM_LINK_CANCELLED : 0u;
}
