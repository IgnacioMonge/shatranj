#include "spectrum/saveload/saveload.h"
#include "spectrum/overlay/overlay.h"

#if SPECTRUM_SAVELOAD_LOAD != SPECTRUM_OVL_SAVELOAD_LOAD_NCZS || \
    SPECTRUM_SAVELOAD_SAVE != SPECTRUM_OVL_SAVELOAD_SAVE_NCZS || \
    SPECTRUM_SAVELOAD_ERASE != SPECTRUM_OVL_SAVELOAD_ERASE_NCZS || \
    SPECTRUM_SAVELOAD_JOURNAL_APPEND != SPECTRUM_OVL_SAVELOAD_JOURNAL_APPEND
#error SAVELOAD operations must match the overlay entry ABI
#endif
#if SPECTRUM_RECOVERY_WRITE != SPECTRUM_OVL_RECOVERY_WRITE || \
    SPECTRUM_RECOVERY_CAPTURE != SPECTRUM_OVL_RECOVERY_CAPTURE || \
    SPECTRUM_RECOVERY_REPLAY != SPECTRUM_OVL_RECOVERY_REPLAY || \
    SPECTRUM_RECOVERY_RETIRE != SPECTRUM_OVL_RECOVERY_RETIRE || \
    SPECTRUM_RECOVERY_RESUME != SPECTRUM_OVL_CONTROL_RECOVERY_RESUME
#error RECOVERY operations must match the overlay entry ABI
#endif

uint8_t spectrum_saveload_run(uint8_t entry, const char *name,
                              const char *buf)
{
    volatile uint8_t *ctx = spectrum_overlay_context;
    uint16_t name_ptr = (uint16_t)name;
    uint16_t buf_ptr = (uint16_t)buf;
    uint8_t overlay = SPECTRUM_OVL_SAVELOAD;

    if (entry & 0x80u) {
        entry &= 0x7fu;
        overlay = SPECTRUM_OVL_CONFIG;
    }
#ifdef NETCHESSZX_RECOVERY
    else if (entry & SPECTRUM_RECOVERY_DISPATCH) {
        overlay = (entry & SPECTRUM_RECOVERY_CONTROL)
            ? SPECTRUM_OVL_CONTROL : SPECTRUM_OVL_RECOVERY;
        entry &= 0x1fu;
    }
#endif
#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_NEXT_BANKING) && \
    !defined(NETCHESSZX_SPECTRANEXT)
    else if (entry < 2u) {
        overlay = SPECTRUM_OVL_JOURNAL;
    }
#endif

    SPECTRUM_OVL_CTX_SET16(SPECTRUM_OVL_CTX_SAVELOAD_NAME_LO,
                           SPECTRUM_OVL_CTX_SAVELOAD_NAME_HI, name_ptr);
    SPECTRUM_OVL_CTX_SET16(SPECTRUM_OVL_CTX_SAVELOAD_BUF_LO,
                           SPECTRUM_OVL_CTX_SAVELOAD_BUF_HI, buf_ptr);
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = 0xffu;
    if (!spectrum_overlay_exec_cached(overlay, entry)) {
        return 0u;
    }
    return (uint8_t)(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] == SPECTRUM_OVL_SAVELOAD_OK);
}

#ifdef NETCHESSZX_RECOVERY
void spectrum_recovery_retire(void)
{
    (void)spectrum_overlay_exec_cached(SPECTRUM_OVL_RECOVERY,
                                       SPECTRUM_RECOVERY_RETIRE);
}

uint8_t spectrum_recovery_capture(uint8_t flags, char *output)
{
    spectrum_overlay_context[SPECTRUM_OVL_CTX_RECOVERY_FLAGS] = flags;
    return spectrum_saveload_run(SPECTRUM_RECOVERY_CAPTURE |
                                     SPECTRUM_RECOVERY_DISPATCH,
                                 output, (const char *)0);
}
#endif
