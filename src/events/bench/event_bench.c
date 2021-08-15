/*
 * Ping-pong test for libevent
 */

#include "event2/event-config.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event.h>
#include <evutil.h>

static int count, writes, fired;
static int *pipes;
static int num_pipes, num_active, num_writes;
static struct event *events;

static void read_cb(evutil_socket_t fd, short which, void *arg) {
    long idx = (long) arg, widx = idx + 1;
    u_char ch;

    count += recv(fd, &ch, sizeof(ch), 0);
    if (writes) {
        if (widx >= num_pipes)
            widx -= num_pipes;
        send(pipes[2 * widx + 1], "e", 1, 0);
        writes--;
        fired++;
    }
}

static struct timeval *run_once(void) {
    int *cp, space;
    long i;
    static struct timeval ta, ts, te;
    gettimeofday(&ta, NULL);

    for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
        if (events[i].ev_base)
            event_del(&events[i]);
        event_set(&events[i], cp[0], EV_READ | EV_PERSIST, read_cb, (void *) i);
        event_add(&events[i], NULL);
    }

    event_loop(EVLOOP_ONCE | EVLOOP_NONBLOCK);

    fired = 0;
    space = num_pipes / num_active;
    space = space * 2;
    for (i = 0; i < num_active; i++, fired++)
        send(pipes[i * space + 1], "e", 1, 0);

    count = 0;
    writes = num_writes;
    {
        int xcount = 0;
        gettimeofday(&ts, NULL);
        do {
            event_loop(EVLOOP_ONCE | EVLOOP_NONBLOCK);
            xcount++;
        } while (count != fired);
        gettimeofday(&te, NULL);

        // if (xcount != count) fprintf(stderr, "Xcount: %d, Rcount: %d\n",
        // xcount, count);
    }

    evutil_timersub(&te, &ta, &ta);
    evutil_timersub(&te, &ts, &ts);
    fprintf(stdout, "total: %8ld ms, loop %8ld ms (%.2f ms/op)\n",
            ta.tv_sec * 1000000L + ta.tv_usec,
            ts.tv_sec * 1000000L + ts.tv_usec,
            (ts.tv_sec * 1000000L + ts.tv_usec) / (double) fired);

    return (&te);
}

int main(int argc, char **argv) {
#ifndef WIN32
    struct rlimit rl;
#endif
    int i, c;
    struct timeval *tv;
    int *cp;

#ifdef WIN32
    WSADATA WSAData;
    WSAStartup(0x101, &WSAData);
#endif
    num_pipes = 100;
    num_active = 1;
    num_writes = num_pipes;
    while ((c = getopt(argc, argv, "n:a:w:")) != -1) {
        switch (c) {
        case 'n':
            num_pipes = atoi(optarg);
            break;
        case 'a':
            num_active = atoi(optarg);
            break;
        case 'w':
            num_writes = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            exit(1);
        }
    }

#ifndef WIN32
    rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        exit(1);
    }
#endif

    events = calloc(num_pipes, sizeof(struct event));
    pipes = calloc(num_pipes * 2, sizeof(int));
    if (events == NULL || pipes == NULL) {
        perror("malloc");
        exit(1);
    }

    event_init();

    for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
#ifdef USE_PIPES
        if (pipe(cp) == -1) {
#else
        if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, cp) == -1) {
#endif
            perror("pipe");
            exit(1);
        }
    }

    fprintf(stdout, "writes: %8d, pipes opened: %8d, active %8d\n", num_writes,
            num_pipes, num_active);
    for (i = 0; i < 25; i++) {
        tv = run_once();
    }

    exit(0);
}
