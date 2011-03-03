#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <unistd.h>

#define NUM_EVENTS 16
#define BUFSIZE 1024

int add_watch(int epfd, int fd, uint32_t events)
{
    struct epoll_event e;
    e.events = events;
    e.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
}

int main(int argc, char** argv)
{
    int pipefd[2];
    int chldfd;
    int pid = 0, err, ep;
    int status;
    bool running = true;
    sigset_t sigset;
    struct epoll_event events[NUM_EVENTS];

    if (argc < 2) {
        fprintf(stderr, "Usage: eppipe command\n"
                "Runs `command` under eppipe\n");
        return 2;
    }
    pipe2(pipefd, O_NONBLOCK);
    pid = fork();
    if (pid == 0) {
        // Close the read end of the pipe
        close(pipefd[0]);
        // Make stdout and stderr
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        err = execvp(argv[1], argv + 1);
        if (err) {
            fprintf(stderr, "Tried to exec '%s'\n", argv[1]);
            perror("execlp");
            return EXIT_FAILURE;
        }
    }
    close(pipefd[1]);
    fcntl(STDOUT_FILENO, F_SETFL, fcntl(STDOUT_FILENO, F_GETFL) | O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDOUT_FILENO, F_GETFL) | O_NONBLOCK);
    // Set up a signalfd to handle the SIGCHLD
    if (sigemptyset(&sigset)) {
        perror("sigemptyset");
        goto kill;
    }
    if (sigaddset(&sigset, SIGCHLD)) {
        perror("sigaddset");
        goto kill;
    }
    if ((chldfd = signalfd(-1, &sigset, SFD_NONBLOCK)) < 0) {
        perror("signalfd");
        goto kill;
    }
    ep = epoll_create(3);
    if (add_watch(ep, STDIN_FILENO, EPOLLIN) < 0) {
        perror("add_watch stdout");
        goto kill;
    }
    if (add_watch(ep, pipefd[0], EPOLLIN) < 0) {
        perror("add_watch pipe");
        goto kill;
    }
    if (add_watch(ep, chldfd, EPOLLIN)) {
        perror("add_watch signalfd");
        goto kill;
    }
    while(running) {
        int n_events, i;
        if ((n_events = epoll_wait(ep, events, NUM_EVENTS, 10000)) < 0) {
            perror("epoll_wait");
            goto kill;
        }
        for (i = 0; i < n_events; ++i) {
            if (events[i].data.fd == STDIN_FILENO) {
                char buf[BUFSIZE];
                ssize_t read_b = 0;
                do {
                    read_b = read(STDIN_FILENO, buf, BUFSIZE);
                } while (read_b > 0);
                if (read_b == 0) {
                    kill(pid, SIGTERM);
                    running = false;
                }
            }
            else if (events[i].data.fd == pipefd[0]) {
                char buf[BUFSIZE];
                ssize_t read_b = 0;
                do {
                    read_b = read(pipefd[0], buf, BUFSIZE);
                    if (read_b > 0) {
                        write(STDOUT_FILENO, buf, read_b);
                    }
                    else if (read_b == 0) {
                        running = false;
                    }
                } while (read_b > 0);
                if (read_b == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("read");
                }
            }
            else if (events[i].data.fd == chldfd) {
                fprintf(stderr, "Got a SIGCHLD without getting a 0-byte read on the pipe\n");
                running = false;
            }
            else {
                printf("Activity on unrecognized fd!\n");
                goto kill;
            }
        }
    }
wait:
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
kill:
    if (pid > 0) {
        kill(pid, SIGTERM);
    }
    goto wait;
}
