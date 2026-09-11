/* Bounded, in-memory tournament coordinator. No gameplay scores live in rooms. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <glib.h>
#include "game.h"
#include "net.h"
#include "log.h"
#include "tournament.h"

#define TOUR_LIMIT 8
#define ENTRANT_LIMIT 16
#define MATCH_LIMIT 15
#define READY_SECONDS 60
#define COUNTDOWN_SECONDS 5
#define REPORT_SECONDS 15
#define RETAIN_SECONDS 1800

enum tour_state { REGISTRATION, RUNNING, COMPLETE, CANCELLED };
enum entrant_state { ENTERED, ALIVE, ELIMINATED, WITHDRAWN, CHAMPION };
enum match_state { WAITING, READY, COUNTDOWN, PLAYING, REPORTING, DISPUTED,
                   MATCH_COMPLETE, BYE, FORFEIT };
static const char *tour_names[] = {"registration", "running", "complete", "cancelled"};
static const char *entrant_names[] = {"entered", "alive", "eliminated", "withdrawn", "champion"};
static const char *match_names[] = {"waiting", "ready", "countdown", "playing", "reporting", "disputed", "complete", "bye", "forfeit"};
struct entrant { int id, fd, ready; enum entrant_state state; char name[11]; };
struct match {
    int id, stage, a, b, wa, wb, round, ready[2], reports[2], room;
    enum match_state state;
    int64_t deadline;
};
struct tournament {
    int id, revision, owner, next_pid, count, stage, champion, slots, matches;
    enum tour_state state;
    int64_t expires;
    struct entrant entrants[ENTRANT_LIMIT];
    struct match bracket[MATCH_LIMIT];
};
static struct tournament tournaments[TOUR_LIMIT];
static int next_tid = 1, watched[256], connected[256], command_count[256];
static int64_t clock_now, rate_window[256];

static void push(int fd, char *line) { send_line_log_push_binary(fd, line, line); }
static void reply(int fd, const char *error) { send_line_log(fd, (char *)error, "TOUR"); }
static struct tournament *find(int id) {
    for (int i = 0; i < TOUR_LIMIT; ++i)
        if (tournaments[i].id == id && id) return &tournaments[i];
    return NULL;
}
static struct entrant *entrant(struct tournament *t, int id) {
    for (int i = 0; i < t->count; ++i) if (t->entrants[i].id == id) return &t->entrants[i];
    return NULL;
}
static struct entrant *self(struct tournament *t, int fd) {
    for (int i = 0; i < t->count; ++i) if (t->entrants[i].fd == fd) return &t->entrants[i];
    return NULL;
}
int tournament_active(int fd) {
    for (int i = 0; i < TOUR_LIMIT; ++i) {
        struct tournament *t = &tournaments[i];
        struct entrant *e = self(t, fd);
        if (t->id && t->state <= RUNNING && e && e->state <= ALIVE) return 1;
    }
    return 0;
}
int tournament_has_running(void) {
    for (int i = 0; i < TOUR_LIMIT; ++i) if (tournaments[i].id && tournaments[i].state == RUNNING) return 1;
    return 0;
}
static void snapshot(struct tournament *t, int fd) {
    char line[8192];
    struct entrant *e = self(t, fd);
    int n = snprintf(line, sizeof(line), "TOUR_STATE: %d %d %s %d %d %d %d ",
                     t->id, t->revision, tour_names[t->state], t->owner, e ? e->id : 0, t->stage, t->champion);
    if (!t->count) n += snprintf(line+n, sizeof(line)-(size_t)n, "-");
    for (int i = 0; i < t->count; ++i) {
        e = &t->entrants[i];
        n += snprintf(line+n, sizeof(line)-(size_t)n, "%s%d,%s,%s,%d", i ? ";" : "", e->id, e->name, entrant_names[e->state], e->ready);
    }
    n += snprintf(line+n, sizeof(line)-(size_t)n, " ");
    if (!t->matches) n += snprintf(line+n, sizeof(line)-(size_t)n, "-");
    for (int i = 0; i < t->matches; ++i) {
        struct match *m = &t->bracket[i];
        int remaining = m->deadline > clock_now ? (int)(m->deadline-clock_now) : 0;
        n += snprintf(line+n, sizeof(line)-(size_t)n, "%s%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d", i ? ";" : "", m->id, m->stage, m->a, m->b, m->wa, m->wb, match_names[m->state], m->round, m->ready[0], m->ready[1], remaining);
    }
    push(fd, line);
}
static void broadcast(struct tournament *t) {
    ++t->revision;
    for (int fd = 0; fd < 256; ++fd)
        if (connected[fd] && (watched[fd] == t->id || self(t, fd))) snapshot(t, fd);
}
static int terminal(struct match *m) { return m->state >= MATCH_COMPLETE; }
static int winner(struct match *m) { return m->wa == 2 ? m->a : m->wb == 2 ? m->b : 0; }
static int available(struct tournament *t, int id) {
    struct entrant *e = entrant(t, id);
    return e && e->fd >= 0 && connected[e->fd] && e->state == ALIVE;
}
static void retire(struct tournament *t, struct match *m) {
    if (m->room) {
        game_tournament_retire(t->id, m->id, m->round);
        m->room = 0;
    }
    m->deadline = 0;
}
static void next_round(struct match *m) {
    ++m->round;
    m->ready[0] = m->ready[1] = 0;
    m->reports[0] = m->reports[1] = -1;
    m->state = READY;
    m->deadline = clock_now + READY_SECONDS;
}
static void eliminate(struct tournament *t, int id) {
    struct entrant *e = entrant(t, id);
    if (e && e->state == ALIVE) e->state = ELIMINATED;
}
static void forfeit(struct tournament *t, struct match *m, int win) {
    retire(t, m);
    m->state = FORFEIT;
    m->wa = win && win == m->a ? 2 : 0;
    m->wb = win && win == m->b ? 2 : 0;
    if (m->a != win) eliminate(t, m->a);
    if (m->b != win) eliminate(t, m->b);
}
/* Stage barriers are applied even to byes and empty subtrees. A withdrawn
 * previous-stage winner is rechecked when its next-stage seat is activated. */
static void advance(struct tournament *t) {
    while (t->state == RUNNING) {
        int all = 1;
        for (int i = 0; i < t->matches; ++i) {
            struct match *m = &t->bracket[i];
            if (m->stage != t->stage) continue;
            if (m->state == WAITING) {
                if (available(t, m->a) && available(t, m->b)) next_round(m);
                else if (!m->a || !m->b) {
                    int win = available(t, m->a) ? m->a : available(t, m->b) ? m->b : 0;
                    m->state = BYE;
                    m->wa = win && win == m->a ? 2 : 0;
                    m->wb = win && win == m->b ? 2 : 0;
                } else forfeit(t, m, available(t, m->a) ? m->a : available(t, m->b) ? m->b : 0);
            }
            if (!terminal(m)) all = 0;
        }
        if (!all) return;
        if (t->bracket[t->matches-1].stage == t->stage) {
            t->champion = winner(&t->bracket[t->matches-1]);
            struct entrant *e = entrant(t, t->champion);
            if (e) e->state = CHAMPION;
            t->state = COMPLETE;
            t->expires = clock_now + RETAIN_SECONDS;
            return;
        }
        int winners[8], n = 0;
        for (int i = 0; i < t->matches; ++i)
            if (t->bracket[i].stage == t->stage) winners[n++] = winner(&t->bracket[i]);
        ++t->stage;
        n = 0;
        for (int i = 0; i < t->matches; ++i) if (t->bracket[i].stage == t->stage) {
            t->bracket[i].a = winners[n++];
            t->bracket[i].b = winners[n++];
        }
    }
}
static void commit_result(struct tournament *t, struct match *m, int win) {
    retire(t, m);
    if (win == m->a && win) ++m->wa;
    if (win == m->b && win) ++m->wb;
    if (m->wa == 2 || m->wb == 2) {
        m->state = MATCH_COMPLETE;
        eliminate(t, m->wa == 2 ? m->b : m->a);
    } else next_round(m);
    advance(t);
}
static void leave(struct tournament *t, struct entrant *e) {
    if (t->state == REGISTRATION) {
        int id = e->id, index = (int)(e-t->entrants);
        memmove(e, e+1, (size_t)(t->count-index-1)*sizeof(*e));
        --t->count;
        if (t->owner == id) t->owner = t->count ? t->entrants[0].id : 0;
        if (!t->count) { t->state = CANCELLED; t->expires = clock_now+RETAIN_SECONDS; }
    } else if (t->state == RUNNING && e->state == ALIVE) {
        e->state = WITHDRAWN;
        for (int i = 0; i < t->matches; ++i) {
            struct match *m = &t->bracket[i];
            if (m->stage == t->stage && !terminal(m) && (m->a == e->id || m->b == e->id)) {
                int other = m->a == e->id ? m->b : m->a;
                forfeit(t, m, available(t, other) ? other : 0);
            }
        }
        advance(t);
    }
    broadcast(t);
}
void tournament_withdraw(int fd) {
    for (int i = 0; i < TOUR_LIMIT; ++i) {
        struct tournament *t = &tournaments[i];
        struct entrant *e = self(t, fd);
        if (t->id && t->state <= RUNNING && e && e->state <= ALIVE) leave(t, e);
    }
}
void tournament_disconnect(int fd) {
    connected[fd] = 0;
    tournament_withdraw(fd);
    for (int i = 0; i < TOUR_LIMIT; ++i) {
        struct entrant *e = self(&tournaments[i], fd);
        if (e) e->fd = -1;
    }
    watched[fd] = 0;
}
void tournament_connect(int fd) {
    connected[fd] = 1;
    watched[fd] = command_count[fd] = 0;
    rate_window[fd] = clock_now;
}
void tournament_tick(int64_t now) {
    clock_now = now;
    for (int i = 0; i < TOUR_LIMIT; ++i) {
        struct tournament *t = &tournaments[i];
        if (!t->id) continue;
        if (t->state >= COMPLETE && now >= t->expires) {
            for (int fd = 0; fd < 256; ++fd) if (watched[fd] == t->id) watched[fd] = 0;
            memset(t, 0, sizeof(*t));
            continue;
        }
        if (t->state != RUNNING) continue;
        int changed = 0;
        for (int j = 0; j < t->matches; ++j) {
            struct match *m = &t->bracket[j];
            if (!m->deadline || now < m->deadline) continue;
            if (m->state == READY) {
                int win = m->ready[0] && available(t, m->a) ? m->a : m->ready[1] && available(t, m->b) ? m->b : 0;
                forfeit(t, m, win);
                changed = 1;
            } else if (m->state == COUNTDOWN) {
                if (!available(t, m->a) || !available(t, m->b))
                    forfeit(t, m, available(t, m->a) ? m->a : available(t, m->b) ? m->b : 0);
                else {
                    m->state = PLAYING;
                    m->deadline = 0;
                    m->room = game_tournament_start(t->id, m->id, m->round, m->a, entrant(t,m->a)->fd, m->b, entrant(t,m->b)->fd);
                    if (!m->room) { m->state = DISPUTED; }
                }
                changed = 1;
            } else if (m->state == REPORTING) {
                retire(t, m);
                m->state = DISPUTED;
                changed = 1;
            }
        }
        if (changed) { advance(t); broadcast(t); }
    }
}
static int number(const char *text, int *out) {
    if (!*text) return 0;
    unsigned long n = 0;
    for (; *text; ++text) {
        if (!isdigit((unsigned char)*text)) return 0;
        n = n*10 + (unsigned)(*text-'0');
        if (n > INT_MAX) return 0;
    }
    *out = (int)n;
    return 1;
}
void tournament_command(int fd, const char *args) {
    char copy[256], *tokens[6], *save, *tok;
    int argc = 0, values[4] = {0};
    if (clock_now-rate_window[fd] >= 10) { rate_window[fd] = clock_now; command_count[fd] = 0; }
    if (++command_count[fd] > 100) { reply(fd, "RATE_LIMITED"); return; }
    if (!args || strlen(args) >= sizeof(copy)) { reply(fd, "BAD_ARGUMENTS"); return; }
    strcpy(copy, args);
    for (tok = strtok_r(copy, " \r\t", &save); tok && argc < 6; tok = strtok_r(NULL, " \r\t", &save)) tokens[argc++] = tok;
    if (!argc || argc > 5) { reply(fd, "BAD_ARGUMENTS"); return; }
    for (int i = 1; i < argc; ++i) if (!number(tokens[i], &values[i-1])) { reply(fd, "BAD_ARGUMENTS"); return; }
    const char *op = tokens[0];
    if (!strcmp(op, "CAPS") && argc == 1) { push(fd, "TOUR_CAPS: 1"); reply(fd,"OK"); return; }
    if (!strcmp(op, "LIST") && argc == 1) {
        char line[1024] = "TOUR_LIST: "; int n = 11, count = 0;
        for (int i = 0; i < TOUR_LIMIT; ++i) if (tournaments[i].id) {
            struct tournament *t = &tournaments[i]; struct entrant *e = entrant(t,t->owner);
            n += snprintf(line+n, sizeof(line)-(size_t)n, "%s%d,%s,%d,%s", count++ ? ";" : "", t->id, tour_names[t->state], t->count, e ? e->name : "-");
        }
        if (!count) strcpy(line+n, "-");
        push(fd,line); reply(fd,"OK"); return;
    }
    if (!strcmp(op,"WATCH") && argc == 2 && !values[0]) { watched[fd] = 0; reply(fd,"OK"); return; }
    if (!strcmp(op,"CREATE") && argc == 1) {
        if (!nick[fd] || is_bot[fd]) { reply(fd,"HUMAN_REQUIRED"); return; }
        if (tournament_active(fd)) { reply(fd,"ALREADY_ENTERED"); return; }
        if (game_has_room(fd)) { reply(fd,"ALREADY_IN_GAME"); return; }
        struct tournament *t = NULL;
        for (int i = 0; i < TOUR_LIMIT; ++i) if (!tournaments[i].id) { t = &tournaments[i]; break; }
        if (!t || next_tid == INT_MAX) { reply(fd,"LIMIT_REACHED"); return; }
        memset(t,0,sizeof(*t)); t->id = next_tid++; t->owner = t->next_pid = 1;
        struct entrant *e = &t->entrants[t->count++];
        e->id = t->next_pid++; e->fd = fd; strcpy(e->name,nick[fd]);
        broadcast(t); reply(fd,"OK"); return;
    }
    int expected = (!strcmp(op,"READY") || !strcmp(op,"RESOLVE")) ? 4 : !strcmp(op,"REPORT") ? 5 : 2;
    if (argc != expected) { reply(fd,"BAD_ARGUMENTS"); return; }
    struct tournament *t = find(values[0]);
    if (!t) { reply(fd,"NOT_FOUND"); return; }
    struct entrant *e = self(t,fd);
    if (!strcmp(op,"STATE") || !strcmp(op,"WATCH")) {
        if (!strcmp(op,"WATCH")) watched[fd] = t->id;
        snapshot(t,fd); reply(fd,"OK"); return;
    }
    if (!strcmp(op,"JOIN")) {
        if (!nick[fd] || is_bot[fd]) { reply(fd,"HUMAN_REQUIRED"); return; }
        if (tournament_active(fd)) { reply(fd,"ALREADY_ENTERED"); return; }
        if (game_has_room(fd)) { reply(fd,"ALREADY_IN_GAME"); return; }
        if (t->state != REGISTRATION) { reply(fd,"NOT_REGISTERING"); return; }
        if (t->count == ENTRANT_LIMIT || t->next_pid == INT_MAX) { reply(fd,"FULL"); return; }
        e = &t->entrants[t->count++]; memset(e,0,sizeof(*e));
        e->id = t->next_pid++; e->fd = fd; strcpy(e->name,nick[fd]);
    } else if (!strcmp(op,"LEAVE")) {
        if (!e) { reply(fd,"NOT_ENTERED"); return; }
        leave(t,e); reply(fd,"OK"); return;
    } else if (!strcmp(op,"CANCEL")) {
        if ((t->state == REGISTRATION && (!e || e->id != t->owner)) || (t->state != REGISTRATION && !admin_authorized[fd])) { reply(fd,"DENIED"); return; }
        for (int i = 0; i < t->matches; ++i) retire(t,&t->bracket[i]);
        t->state = CANCELLED; t->expires = clock_now+RETAIN_SECONDS;
    } else if (!strcmp(op,"START")) {
        if (!e || e->id != t->owner) { reply(fd,"DENIED"); return; }
        if (t->state != REGISTRATION) { reply(fd,"NOT_REGISTERING"); return; }
        if (t->count < 4) { reply(fd,"MIN_PLAYERS"); return; }
        for (int i = 0; i < t->count; ++i) if (!t->entrants[i].ready || !connected[t->entrants[i].fd]) { reply(fd,"NOT_READY"); return; }
        int order[ENTRANT_LIMIT];
        for (int i = 0; i < t->count; ++i) { order[i] = t->entrants[i].id; t->entrants[i].state = ALIVE; }
        for (int i = t->count-1; i > 0; --i) { int j = g_random_int_range(0,i+1), tmp = order[i]; order[i] = order[j]; order[j] = tmp; }
        t->slots = t->count <= 4 ? 4 : t->count <= 8 ? 8 : 16; t->matches = t->slots-1; t->state = RUNNING;
        int idx = 0;
        for (int stage = 0, size = t->slots/2; size; ++stage, size /= 2)
            for (int i = 0; i < size; ++i) { struct match *m = &t->bracket[idx]; m->id = ++idx; m->stage = stage; }
        /* One entrant per opening match first, then distribute second seats
         * across both bracket halves with bit-reversed match indices. */
        int opening = t->slots/2;
        for (int i = 0; i < opening; ++i) t->bracket[i].a = order[i];
        for (int i = 0; i < t->count-opening; ++i) {
            int rev = 0; for (int bits = opening, x = i; bits > 1; bits /= 2, x /= 2) rev = rev*2+(x&1);
            t->bracket[rev].b = order[opening+i];
        }
        advance(t);
    } else if (!strcmp(op,"READY") && t->state == REGISTRATION) {
        if (!e || values[1] || values[2]) { reply(fd,"NOT_ASSIGNED"); return; }
        if (e->ready) { reply(fd,"OK"); return; }
        e->ready = 1;
    } else if (!strcmp(op,"READY") || !strcmp(op,"REPORT") || !strcmp(op,"RESOLVE")) {
        if (t->state != RUNNING || values[1] <= 0 || values[1] > t->matches) { reply(fd,"NOT_ACTIVE"); return; }
        struct match *m = &t->bracket[values[1]-1];
        int side = e && e->id == m->a ? 0 : e && e->id == m->b ? 1 : -1;
        if (!strcmp(op,"RESOLVE")) {
            if (!admin_authorized[fd]) { reply(fd,"DENIED"); return; }
            if (m->state != DISPUTED) { reply(fd,"NOT_DISPUTED"); return; }
            int win = values[2];
            if (win && win != m->a && win != m->b) { reply(fd,"INVALID_WINNER"); return; }
            l4(OUTPUT_TYPE_INFO,"Tournament administrative resolution: tournament=%d match=%d administrator_fd=%d winner=%d",t->id,m->id,fd,win);
            commit_result(t,m,win);
        } else {
            if (side < 0) { reply(fd,"NOT_ASSIGNED"); return; }
            if (values[2] != m->round) { reply(fd,"STALE_ROUND"); return; }
            if (!strcmp(op,"READY")) {
                if (m->state != READY && m->state != COUNTDOWN) { reply(fd,"NOT_READY_PHASE"); return; }
                if (m->ready[side]) { reply(fd,"OK"); return; }
                m->ready[side] = 1;
                if (m->ready[0] && m->ready[1]) { m->state = COUNTDOWN; m->deadline = clock_now+COUNTDOWN_SECONDS; }
            } else {
                int win = values[3];
                if (win && win != m->a && win != m->b) { reply(fd,"INVALID_WINNER"); return; }
                if (m->reports[side] >= 0) { reply(fd,m->reports[side] == win ? "OK" : "REPORT_IMMUTABLE"); return; }
                if (m->state != PLAYING && m->state != REPORTING) { reply(fd,"NOT_PLAYING"); return; }
                m->reports[side] = win;
                if (m->reports[1-side] >= 0) {
                    if (m->reports[1-side] == win) commit_result(t,m,win);
                    else { retire(t,m); m->state = DISPUTED; }
                } else { m->state = REPORTING; m->deadline = clock_now+REPORT_SECONDS; }
            }
        }
    } else { reply(fd,"UNKNOWN_OPERATION"); return; }
    broadcast(t); reply(fd,"OK");
}
