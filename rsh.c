#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

#define N 13
extern char **environ;
char uName[20];

char *allowed[N] = {
    "cp","touch","mkdir","ls","pwd","cat","grep",
    "chmod","diff","cd","exit","help","sendmsg"
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

void sendmsg (char *user, char *target, char *msg) {
    struct message m;
    strncpy(m.source, user, sizeof(m.source)-1);
    m.source[sizeof(m.source)-1] = '\0';
    strncpy(m.target, target, sizeof(m.target)-1);
    m.target[sizeof(m.target)-1] = '\0';
    strncpy(m.msg, msg, sizeof(m.msg)-1);
    m.msg[sizeof(m.msg)-1] = '\0';

    int fd = open("serverFIFO", O_WRONLY);
    if (fd < 0) {
        perror("sendmsg: open serverFIFO");
        return;
    }
    if (write(fd, &m, sizeof(m)) < 0) {
        perror("sendmsg: write to serverFIFO");
    }
    close(fd);
}

void* messageListener(void *arg) {
    // listen on this user's FIFO and print any incoming message
    char *fifo = uName;
    while (1) {
        int fd = open(fifo, O_RDONLY);
        if (fd < 0) {
            perror("messageListener: open FIFO");
            sleep(1);
            continue;
        }
        struct message m;
        ssize_t n;
        while ((n = read(fd, &m, sizeof(m))) > 0) {
            printf("\nIncoming message from %s: %s\n", m.source, m.msg);
            fflush(stdout);
        }
        close(fd);
    }
    return NULL;
}

int isAllowed(const char*cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);
    strcpy(uName, argv[1]);

    // make sure our FIFO exists
    if (mkfifo(uName, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo user FIFO");
        exit(1);
    }
    // start listener thread
    pthread_t tid;
    if (pthread_create(&tid, NULL, messageListener, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh> ");
        if (fgets(line, sizeof(line), stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;
        line[strlen(line)-1] = '\0';

        char cmd[256], line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

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
            char *message = strtok(NULL, "");
            if (!message) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg(uName, target, message);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;
        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }
        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i+1, allowed[i]);
            }
            continue;
        }

        // all other allowed external commands
        strcpy(line2, line);
        cargv = NULL;
        int argc2 = 0;
        char *tok = strtok(line2, " ");
        while (tok) {
            cargv = realloc(cargv, sizeof(char*)*(argc2+1));
            cargv[argc2++] = strdup(tok);
            tok = strtok(NULL, " ");
        }
        cargv = realloc(cargv, sizeof(char*)*(argc2+1));
        cargv[argc2] = NULL;

        posix_spawnattr_init(&attr);
        path = cargv[0];

        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }
        posix_spawnattr_destroy(&attr);

        for (int i = 0; i < argc2; i++) free(cargv[i]);
        free(cargv);
    }

    return 0;
}
