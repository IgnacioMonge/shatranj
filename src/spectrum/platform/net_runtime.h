#ifndef NETCHESSZX_SPECTRUM_NET_RUNTIME_H
#define NETCHESSZX_SPECTRUM_NET_RUNTIME_H

#include <stdint.h>

/* Narrow transport -> app bridge. It lives under platform/ so transport can call
   timing/UI composition hooks without including app/ or ui/ directly. Keep this
   surface small; publish_ip_status is ABI compatibility and must not paint the
   status bar. */
void spectrum_net_runtime_wait_frame(void);
void spectrum_net_runtime_wait_frame_plain(void);
void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second);
uint8_t spectrum_net_runtime_clock_ready(void);
void spectrum_net_runtime_publish_ip_status(const char *ip);

#endif
