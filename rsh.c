#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {
    "cp","touch","mkdir","ls","pwd","cat","grep","chmod",
    "diff","cd","exit","help","sendmsg"
};

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

/* TODO: send a request to the server to forward a message */
void sendmsg(char *user, char *target, char *msg) {
    struct message m;
    strncpy(m.source, user, sizeof(m.source)-1);
    m.source[sizeof(m.source)-1] = '\0';
    strncpy(m.target, target, sizeof(m.target)-1);
    m.target[sizeof(m.target)-1] = '\0';
    strncpy(m.msg, msg, sizeof(m.msg)-1);
    m.msg[sizeof(m.msg)-1] = '\0';

    int fd = open("serverFIFO", O_WRONLY);
    if (fd < 0) {
        perror("open serverFIFO");
        return;
    }
    if (write(fd, &m, sizeof(m)) < 0) {
        perror("write to serverFIFO");
    }
    close(fd);
}

/* TODO: background thread that listens on your FIFO for incoming messages */
void* messageListener(void *arg) {
    char *user = (char*)arg;
    struct message m;
    int fd = open(user, O_RDONLY);
    if (fd < 0) {
        perror("open user FIFO");
        pthread_exit(NULL);
    }
    while (1) {
        ssize_t n = read(fd, &m, sizeof(m));
        if (n > 0) {
            printf("Incoming message from %s: %s\n", m.source, m.msg);
            printf("rsh>"); fflush(stdout);
        }
    }
    close(fd);
    pthread_exit(NULL);
}

int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    pthread_t tid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        exit(1);
    }
    signal(SIGINT, terminate);
    strncpy(uName, argv[1], sizeof(uName)-1);
    uName[sizeof(uName)-1] = '\0';

    if (pthread_create(&tid, NULL, messageListener, uName) != 0) {
        perror("pthread_create");
        exit(1);
    }

    while (1) {
        printf("rsh>"); fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        if (strcmp(line, "\n") == 0) continue;
        line[strlen(line)-1] = '\0';

        char line2[256];
        strcpy(line2, line);
        char *cmd = strtok(line, " ");

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }
            char *p = strchr(line2, ' ');
            if (p) p = strchr(p+1, ' ');
            if (!p) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg(uName, target, p+1);
            continue;
        }

        if (strcmp(cmd, "cd") == 0) {
            char *dir = strtok(NULL, " ");
            if (!dir) {
                printf("-rsh: cd: missing argument\n");
            } else if (strtok(NULL, " ")) {
                printf("-rsh: cd: too many arguments\n");
            } else if (chdir(dir) < 0) {
                perror("cd");
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("  %s\n", allowed[i]);
            }
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        int argc2 = 0;
        char *tok = strtok(line2, " ");
        cargv = NULL;
        while (tok) {
            cargv = realloc(cargv, sizeof(char*)*(argc2+1));
            cargv[argc2++] = strdup(tok);
            tok = strtok(NULL, " ");
        }
        cargv = realloc(cargv, sizeof(char*)*(argc2+1));
        cargv[argc2] = NULL;

        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, cargv[0], NULL, &attr, cargv, environ) < 0) {
            perror("spawn failed");
        } else {
            waitpid(pid, &status, 0);
        }
        posix_spawnattr_destroy(&attr);
        for (int i = 0; i < argc2; i++) free(cargv[i]);
        free(cargv);
    }
    return 0;
}
