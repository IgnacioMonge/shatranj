#ifndef NETCHESSZX_TEST_SPECTRANEXT_API_FIXTURE_H
#define NETCHESSZX_TEST_SPECTRANEXT_API_FIXTURE_H

#include <stdint.h>

/* Host declarations shared by the SDK mocks and independent overlay checks. */
#define SPXN_OK          0
#define SPXN_EROM       -1
#define SPXN_EINVAL     -6
#define SPXN_ESENDSTALL -8
#define SPXN_POLLCON     1
#define SPXN_POLLHUP     2
#define SPXN_POLLIN      4
#define SPXN_POLLNVAL  128

struct spxn_status {
    uint8_t controller;
    uint8_t wifi;
    uint8_t ip4host[4];
};

struct spxtime_result {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t fat_date;
    uint16_t fat_time;
};

struct spxtime_request {
    const char *host;
    uint8_t *scratch;
    uint16_t scratch_size;
    uint16_t timeout_ticks;
    struct spxtime_result *out;
};

struct spxudp_endpoint {
    uint8_t ip4be[4];
    uint16_t port;
    uint16_t local_port;
};

struct spxudp_socket {
    uint8_t fd;
    uint8_t open;
    uint8_t poll_in;
};

#define SPXTIME_SNTP_PACKET_SIZE 48u
#define SPXTIME_SNTP_PORT 123u

int16_t spxn_status(struct spxn_status *out);
int16_t spxn_detect(void);
int16_t spxn_poll(void);
int16_t spxn_recv(void *buffer, uint16_t maximum);
int16_t spxn_accept(void);
int16_t spxn_accept_single(void);
int16_t spxn_listen(uint16_t port);
int16_t spxn_resolve(const char *host, uint8_t *ip4be);
int16_t spxn_connect(const uint8_t *ip4be, uint16_t port);
void spxn_close(void);
int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget);
int16_t spxtime_sntp(const struct spxtime_request *request);
int16_t spxtime_parse_sntp(const uint8_t *packet, uint16_t length,
                           struct spxtime_result *out);
int16_t spxudp_open(struct spxudp_socket *socket);
int16_t spxudp_sendto(struct spxudp_socket *socket, const void *buffer,
                      uint16_t length,
                      const struct spxudp_endpoint *destination);
int16_t spxudp_poll(struct spxudp_socket *socket);
int16_t spxudp_recvfrom(struct spxudp_socket *socket, void *buffer,
                        uint16_t maximum,
                        struct spxudp_endpoint *source);
void spxudp_close(struct spxudp_socket *socket);

#endif
