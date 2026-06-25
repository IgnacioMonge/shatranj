#ifndef NETCHESSZX_COMMON_GAME_PROTOCOL_H
#define NETCHESSZX_COMMON_GAME_PROTOCOL_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *netchess_after_prefix(const char *text, const char *prefix);

uint8_t netchess_proto_parse_move(const char *rx,
                                  char *ply,
                                  uint8_t ply_cap,
                                  char *move,
                                  uint8_t move_cap,
                                  char *notation,
                                  uint8_t notation_cap);
uint8_t netchess_proto_parse_chat(const char *rx,
                                  char *text,
                                  uint8_t text_cap);
uint8_t netchess_proto_parse_ack(const char *rx,
                                 char *ply,
                                 uint8_t ply_cap,
                                 char *notation,
                                 uint8_t notation_cap);
uint8_t netchess_proto_parse_nack(const char *rx,
                                  char *ply,
                                  uint8_t ply_cap,
                                  char *reason,
                                  uint8_t reason_cap);
uint8_t netchess_proto_parse_game_start(const char *rx,
                                        char *detail,
                                        uint8_t detail_cap);
uint8_t netchess_proto_is_ack(const char *rx) NETCHESSZX_FASTCALL;
uint8_t netchess_proto_is_nack(const char *rx) NETCHESSZX_FASTCALL;
uint8_t netchess_proto_is_reset(const char *rx) NETCHESSZX_FASTCALL;
uint8_t netchess_proto_is_bye(const char *rx) NETCHESSZX_FASTCALL;

uint8_t netchess_proto_format_move(char *out,
                                   uint8_t out_cap,
                                   const char *ply,
                                   const char *move,
                                   const char *notation);
uint8_t netchess_proto_format_chat(char *out,
                                   uint8_t out_cap,
                                   const char *text);
uint8_t netchess_proto_format_ack(char *out,
                                  uint8_t out_cap,
                                  const char *ply,
                                  const char *notation);
uint8_t netchess_proto_format_nack(char *out,
                                   uint8_t out_cap,
                                   const char *ply,
                                   const char *reason);
uint8_t netchess_proto_format_game_start(char *out,
                                         uint8_t out_cap,
                                         const char *detail);
uint8_t netchess_proto_format_reset(char *out, uint8_t out_cap);
uint8_t netchess_proto_format_bye(char *out, uint8_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
