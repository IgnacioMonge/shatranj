#ifndef NETCHESSZX_COMMON_TEXT_H
#define NETCHESSZX_COMMON_TEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Return the suffix after a complete prefix, or NULL on mismatch.
   Implemented in common/protocol/game_protocol.c; keep that translation unit
   linked for users of this declaration; the desktop glob skips C files at the
   root of common. */
const char *netchess_after_prefix(const char *text, const char *prefix);
/* Nonzero only when text equals token; implemented beside the above. */
unsigned char netchess_text_is(const char *text, const char *token);

#ifdef __cplusplus
}
#endif

#endif
