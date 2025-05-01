#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

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

void sendmsg(char *user, char *target, char *msg) {
    struct message msg_struct;
    strncpy(msg_struct.source, user, sizeof(msg_struct.source)-1);
    strncpy(msg_struct.target, target, sizeof(msg_struct.target)-1);
    strncpy(msg_struct.msg, msg, sizeof(msg_struct.msg)-1);
    msg_struct.source[sizeof(msg_struct.source)-1] = '\0';
    msg_struct.target[sizeof(msg_struct.target)-1] = '\0';
    msg_struct.msg[sizeof(msg_struct.msg)-1] = '\0';

    int server_fd = open("serverFIFO", O_WRONLY);
    if (server_fd == -1) {
        perror("sendmsg: open serverFIFO failed");
        return;
    }
    write(server_fd, &msg_struct, sizeof(msg_struct));
    close(server_fd);
}

void* messageListener(void *arg) {
    int fd = open(uName, O_RDONLY); // Open FIFO once
    if (fd == -1) {
        perror("messageListener: open FIFO failed");
        pthread_exit(NULL);
    }

    while (1) {
        struct message incoming;
        ssize_t bytes = read(fd, &incoming, sizeof(incoming));
        if (bytes == sizeof(incoming)) {
            printf("Incoming message from %s: %s\n", incoming.source, incoming.msg);
            fflush(stdout);
        } else if (bytes <= 0) {
            // Reopen FIFO if closed by server
            close(fd);
            fd = open(uName, O_RDONLY);
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
    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);
    strncpy(uName, argv[1], sizeof(uName)-1);
    uName[sizeof(uName)-1] = '\0';

    // Message listener thread
    pthread_t tid;
    pthread_create(&tid, NULL, messageListener, NULL);
    pthread_detach(tid);

    while (1) {
        printf("rsh>");
        fflush(stdout);

        char line[256];
        if (!fgets(line, sizeof(line), stdin)) continue;
        line[strcspn(line, "\n")] = '\0'; // Remove newline

        if (strlen(line) == 0) continue;

        char *cmd = strtok(line, " ");
        if (!cmd) continue;

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            char *message = target ? strtok(NULL, "\n") : NULL;

            if (!target) {
                printf("sendmsg: specify target user\n");
                continue;
            }
            if (!message || strlen(message) == 0) {
                printf("sendmsg: enter a message\n");
                continue;
            }
            sendmsg(uName, target, message);
            continue;
        }


	if (strcmp(cmd,"exit")==0) break;

	if (strcmp(cmd,"cd")==0) {
		char *targetDir=strtok(NULL," ");
		if (strtok(NULL," ")!=NULL) {
			printf("-rsh: cd: too many arguments\n");
		}
		else {
			chdir(targetDir);
		}
		continue;
	}

	if (strcmp(cmd,"help")==0) {
		printf("The allowed commands are:\n");
		for (int i=0;i<N;i++) {
			printf("%d: %s\n",i+1,allowed[i]);
		}
		continue;
	}

	cargv = (char**)malloc(sizeof(char*));
	cargv[0] = (char *)malloc(strlen(cmd)+1);
	path = (char *)malloc(9+strlen(cmd)+1);
	strcpy(path,cmd);
	strcpy(cargv[0],cmd);

	char *attrToken = strtok(line2," "); /* skip cargv[0] which is completed already */
	attrToken = strtok(NULL, " ");
	int n = 1;
	while (attrToken!=NULL) {
		n++;
		cargv = (char**)realloc(cargv,sizeof(char*)*n);
		cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
		strcpy(cargv[n-1],attrToken);
		attrToken = strtok(NULL, " ");
	}
	cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
	cargv[n] = NULL;

	// Initialize spawn attributes
	posix_spawnattr_init(&attr);

	// Spawn a new process
	if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
		perror("spawn failed");
		exit(EXIT_FAILURE);
	}

	// Wait for the spawned process to terminate
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid failed");
		exit(EXIT_FAILURE);
	}

	// Destroy spawn attributes
	posix_spawnattr_destroy(&attr);

    }
    return 0;
}
