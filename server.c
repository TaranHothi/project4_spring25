#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server, dummyfd;
    struct message req;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    server = open("serverFIFO", O_RDONLY);
    if (server < 0) {
        perror("open serverFIFO");
        exit(1);
    }
    dummyfd = open("serverFIFO", O_WRONLY);
    if (dummyfd < 0) {
        perror("open dummy write end");
        exit(1);
    }

    while (1) {
        ssize_t n = read(server, &req, sizeof(req));
        if (n != sizeof(req)) continue;

        printf("Received a request from %s to send the message %s to %s.\n",
               req.source, req.msg, req.target);

        int target_fd = open(req.target, O_WRONLY);
        if (target_fd < 0) {
            perror("open target FIFO");
            continue;
        }
        if (write(target_fd, &req, sizeof(req)) < 0) {
            perror("write to target FIFO");
        }
        close(target_fd);
    }

    close(server);
    close(dummyfd);
    return 0;
}
