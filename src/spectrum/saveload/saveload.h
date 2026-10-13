#ifndef NETCHESSZX_SPECTRUM_SAVELOAD_H
#define NETCHESSZX_SPECTRUM_SAVELOAD_H

#include <stdint.h>

/* SAVELOAD operations; the dispatcher checks these against its entry ABI. */
#define SPECTRUM_SAVELOAD_LOAD 0u
#define SPECTRUM_SAVELOAD_SAVE 1u
#define SPECTRUM_SAVELOAD_ERASE 2u
#define SPECTRUM_SAVELOAD_JOURNAL_APPEND 3u
/* Recovery operations: RECOVERY entries, plus the cold resume entry that
   lives in CONTROL (RECOVERY_CONTROL selects that overlay). REPLAY is
   RECOVERY entry 2, the same number as RESUME in CONTROL. */
#define SPECTRUM_RECOVERY_WRITE 0u
#define SPECTRUM_RECOVERY_CAPTURE 1u
#define SPECTRUM_RECOVERY_RESUME 2u
#define SPECTRUM_RECOVERY_REPLAY 2u
#define SPECTRUM_RECOVERY_RETIRE 3u
#define SPECTRUM_RECOVERY_CONTROL 0x20u
#define SPECTRUM_RECOVERY_DISPATCH 0x40u
#define SPECTRUM_JOURNAL_LOAD SPECTRUM_SAVELOAD_LOAD
#define SPECTRUM_JOURNAL_SAVE SPECTRUM_SAVELOAD_SAVE

#define SPECTRUM_CONFIG_RECORD_SIZE 58u
#define SPECTRUM_CONFIG_STATE_SAVED 3u
#define SPECTRUM_CONFIG_STATE_INVALID 4u
#define SPECTRUM_CONFIG_STATE_IO 5u

uint8_t spectrum_saveload_run(uint8_t entry, const char *name,
                              const char *buf);

#define spectrum_saveload_write(name, b64) \
    spectrum_saveload_run(SPECTRUM_SAVELOAD_SAVE, (name), (b64))
#define spectrum_saveload_read(name, b64) \
    spectrum_saveload_run(SPECTRUM_SAVELOAD_LOAD, (name), (b64))
#define spectrum_saveload_erase(name) \
    spectrum_saveload_run(SPECTRUM_SAVELOAD_ERASE, (name), \
                          (const char *)0)
#ifdef NETCHESSZX_RECOVERY
/* b64 == 0 probes for an offerable checkpoint; otherwise stages it (snapshot
   into b64, linked history into SHATJRNL.NEW). See rcv_resume.asm. */
#define spectrum_recovery_read(b64) \
    spectrum_saveload_run(SPECTRUM_RECOVERY_RESUME | \
                              SPECTRUM_RECOVERY_CONTROL | \
                              SPECTRUM_RECOVERY_DISPATCH, \
                          (const char *)0, (b64))
#define spectrum_recovery_write(b64) \
    spectrum_saveload_run(SPECTRUM_RECOVERY_WRITE | SPECTRUM_RECOVERY_DISPATCH, \
                          (const char *)0, (b64))
/* Encodes the live position: output == 0 writes a checkpoint, otherwise
   the payload goes to output (SAVE) and nothing is written. */
uint8_t spectrum_recovery_capture(uint8_t flags, char *output);
/* Recovery is for interruptions. After a chosen exit by either player (the
   menu disconnect, or the BYE the peer sends) RECOVERY writes a live game as
   the newest, never offered, OVER checkpoint. A lost link, a reset or a power
   cut keeps the offer. */
void spectrum_recovery_retire(void);
/* SpectraNext: paint SHATJRNL.NEW into the move list (RECOVERY entry 2).
   ctx[0..1] = 3 when black is to move (odd ply: a marker takes the white
   column), 1 when white is; nonzero once the list holds the history or the
   RESTORED marker. */
#define spectrum_recovery_replay(black_to_move) \
    spectrum_saveload_run(SPECTRUM_RECOVERY_REPLAY | \
                              SPECTRUM_RECOVERY_DISPATCH, \
                          (const char *)(uintptr_t)((black_to_move) ? 3u : 1u), \
                          (const char *)0)
#else
#define spectrum_recovery_read(b64) ((void)(b64), 0u)
#define spectrum_recovery_write(b64) ((void)(b64), 1u)
#define spectrum_recovery_capture(flags, output) \
    ((void)(flags), (void)(output), 1u)
#define spectrum_recovery_replay(black_to_move) ((void)(black_to_move), 0u)
#define spectrum_recovery_retire() ((void)0)
#endif
/* Returns 0 for no file, SAVED, INVALID, or IO. */
uint8_t netchesszx_config_load_overlay(void);
uint8_t netchesszx_config_save_overlay(void);
uint8_t netchesszx_config_defaults_overlay(void);

#endif
