#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

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
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    // Create server FIFO if missing
    mkfifo("serverFIFO", 0666);

    // Open FIFOs in non-blocking mode
    int server_fd = open("serverFIFO", O_RDONLY | O_NONBLOCK);
    int dummy_fd = open("serverFIFO", O_WRONLY | O_NONBLOCK);

    while (1) {
        struct message req;
        ssize_t bytes = read(server_fd, &req, sizeof(req));

        if (bytes == sizeof(req)) {
            // Forward message to target
            int target_fd = open(req.target, O_WRONLY | O_NONBLOCK);
            if (target_fd != -1) {
                write(target_fd, &req, sizeof(req));
                close(target_fd);
            }
        }
        usleep(100000); // Prevent CPU hogging
    }

    close(server_fd);
    close(dummy_fd);
    return 0;
}
