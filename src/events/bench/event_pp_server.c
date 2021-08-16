/*
 * Ping-pong benchmark (server)
 */
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_tcp_no_delay(evutil_socket_t fd)
{
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
}

static void signal_cb(evutil_socket_t fd, short what, void * arg)
{
    struct event_base * base = arg;
    printf("stop\n");

    event_base_loopexit(base, NULL);
}

static void echo_read_cb(struct bufferevent * bev, void * ctx)
{
    /* This callback is invoked when there is data to read on bev. */
    struct evbuffer * input = bufferevent_get_input(bev);
    struct evbuffer * output = bufferevent_get_output(bev);

    /* Copy all the data from the input buffer to the output buffer. */
    evbuffer_add_buffer(output, input);
}

static void echo_event_cb(struct bufferevent * bev, short events, void * ctx)
{
    struct evbuffer * output = bufferevent_get_output(bev);
    size_t remain = evbuffer_get_length(output);
    if (events & BEV_EVENT_ERROR)
    {
        perror("Error from bufferevent");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
    {
        printf("closing, remain %zd\n", remain);
        bufferevent_free(bev);
    }
}

static void accept_conn_cb(struct evconnlistener * listener, evutil_socket_t fd, struct sockaddr * address, int socklen, void * ctx)
{
    /* We got a new connection! Set up a bufferevent for it. */
    struct event_base * base = evconnlistener_get_base(listener);
    struct bufferevent * bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    set_tcp_no_delay(fd);

    bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, NULL);

    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

struct event_base * server_base;

void * server_thread(int * port)
{
    struct evconnlistener * listener;
    struct sockaddr_in sin;
    struct event * evstop;

    server_base = event_base_new();
    if (!server_base)
    {
        char * s = "Couldn't open event base";
        puts(s);
        return (void *)s;
    }

    evstop = evsignal_new(server_base, SIGHUP, signal_cb, server_base);
    evsignal_add(evstop, NULL);

    /* Clear the sockaddr before using it, in case there are extra
     *          * platform-specific fields that can mess us up. */
    memset(&sin, 0, sizeof(sin));
    /* This is an INET address */
    sin.sin_family = AF_INET;
    /* Listen on 0.0.0.0 */
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* Listen on the given port. */
    sin.sin_port = htons((unsigned short)*port);

    listener = evconnlistener_new_bind(
        server_base, accept_conn_cb, NULL, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr *)&sin, sizeof(sin));
    if (!listener)
    {
        char * s = "Couldn't create listener";
        puts(s);
        return (void *)s;
    }

    event_base_dispatch(server_base);

    evconnlistener_free(listener);
    event_free(evstop);
    event_base_free(server_base);

    return NULL;
}
