#ifndef FB_TOURNAMENT_H
#define FB_TOURNAMENT_H
#include <stdint.h>
/* The event loop supplies monotonic seconds, also allowing deterministic core tests. */
void tournament_tick(int64_t now);
void tournament_command(int fd, const char *args);
void tournament_disconnect(int fd);
void tournament_connect(int fd);
int tournament_active(int fd);
void tournament_withdraw(int fd);
int tournament_has_running(void);
#endif
