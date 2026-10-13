#ifndef NETCHESSZX_SPECTRUM_SESSION_OUTGOING_H
#define NETCHESSZX_SPECTRUM_SESSION_OUTGOING_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

uint8_t netchesszx_session_send_ack_move(const char *ply);
uint8_t netchesszx_session_send_nack_move(const char *ply);
uint8_t netchesszx_session_send_text(const char *text) NETCHESSZX_FASTCALL;
uint8_t netchesszx_session_send_control_text(const char *text)
    NETCHESSZX_FASTCALL;
uint8_t netchesszx_session_send_ack_text(const char *text)
    NETCHESSZX_FASTCALL;
uint8_t netchesszx_session_send_ping(void);
uint8_t netchesszx_session_send_ack_ping(void);
uint8_t netchesszx_session_send_start_game(void);

#endif
