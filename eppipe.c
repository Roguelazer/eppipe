/*
 * Copyright (c) 2011 Yelp, Inc.
 *
 * Available under the ISC License. For details, see
 * LICENSE
 */

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAS_SIGNALFD
#include <sys/signalfd.h>
#endif
#include <unistd.h>

#define NUM_EVENTS 16

static int pid = 0;
static bool running = true;

/* Handler for SIGCHLD */
void handler(int signal)
{
    (void) signal;
    running = false;
}

int add_watch(int epfd, int fd, uint32_t events)
{
    struct epoll_event e;
    e.events = events;
    e.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
}

int main(int argc, char** argv)
{
    int err, ep;
    int status;
#ifdef HAS_SIGNALFD
    int chldfd;
#endif
    sigset_t sigset;
    struct epoll_event events[NUM_EVENTS];

    if (argc < 2) {
        fprintf(stderr, "Usage: eppipe command\n"
                "Runs `command` under eppipe\n");
        return 2;
    }
    pid = fork();
    if (pid == 0) {
        err = execvp(argv[1], argv + 1);
        if (err) {
            fprintf(stderr, "Tried to exec '%s'\n", argv[1]);
            perror("execlp");
            return EXIT_FAILURE;
        }
    } else if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }
    // Set up a signalfd to handle the SIGCHLD
    if (sigemptyset(&sigset)) {
        perror("sigemptyset");
        goto kill;
    }
    if (sigaddset(&sigset, SIGCHLD)) {
        perror("sigaddset");
        goto kill;
    }
#ifdef HAS_SIGNALFD
    if ((chldfd = signalfd(-1, &sigset, SFD_NONBLOCK)) < 0) {
        perror("signalfd");
        goto kill;
    }
#else
    signal(SIGCHLD, handler);
#endif
    if ((ep = epoll_create(3)) < 0) {
        perror("epoll_create");
        goto kill;
    }
    if (add_watch(ep, STDOUT_FILENO, EPOLLIN|EPOLLHUP) < 0) {
        perror("add_watch stdout");
        goto kill;
    }
#ifdef HAS_SIGNALFD
    if (add_watch(ep, chldfd, EPOLLIN)) {
        perror("add_watch signalfd");
        goto kill;
    }
#endif
    while(running) {
        int n_events, i;
#ifdef HAS_SIGNALFD
        if ((n_events = epoll_wait(ep, events, NUM_EVENTS, 10000)) < 0) {
#else
        if ((n_events = epoll_pwait(ep, events, NUM_EVENTS, 100, &sigset)) < 0) {
#endif
            perror("epoll_wait");
            goto kill;
        }
        for (i = 0; i < n_events; ++i) {
            if (events[i].data.fd == STDOUT_FILENO) {
                char buf[1];
                ssize_t read_b = 0;
                do {
                    read_b = read(STDOUT_FILENO, buf, 1);
                } while (read_b > 0);
                if (read_b == 0) {
                    kill(pid, SIGTERM);
                    running = false;
                    break;
                }
            }
#ifdef HAS_SIGNALFD
            else if (events[i].data.fd == chldfd) {
                running = false;
            }
#endif
            else {
                printf("Activity on unrecognized fd!\n");
                goto kill;
            }
        }
    }
wait:
    fflush(stdout);
    fflush(stderr);
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
kill:
    if (pid > 0) {
        kill(pid, SIGTERM);
    }
    goto wait;
}
