#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#define FORCE_SMALL_DELAY 0
#define FORCE_BIG_DELAY 0

#if FORCE_BIG_DELAY
#include <time.h>
static const struct timespec BIG_DELAY = { 0, 1000 };
#endif

#define FORCE_USE_PIPE 0

typedef struct {
    pid_t pid;
    int fd;
    int *enable_sleep;
} child_harvest_params;

static int comm_socketpair(int sv[2]) {
#if FORCE_USE_PIPE
    int rc = pipe(sv);
#else
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
#endif
    if (rc < 0)
        return -1;
    return 0;
}

static void* child_harvester_thread(void* args) {
    int status;
    child_harvest_params* params = (child_harvest_params*) args;

    waitpid(params->pid, &status, 0);
    if (close(params->fd)) {
        printf("close() error.\n");
    } else {
        printf("File descriptor closed.\n");
    }
    free(params);
    return NULL;
}

static const int MAX_WRITE_VALUE = 100000;

int get_fd() {
    
    pid_t pid;
    int s[2];

    if (comm_socketpair(s)) {
        printf("Failure to create socket pair for communication.\n");
        return -1;
    }

    fcntl(s[0], F_SETFD, FD_CLOEXEC);

    pid = fork();
    if (pid < 0) {
        printf("Failure on fork().\n");
        close(s[0]);
        close(s[1]);
        return -1;
    }

    if (pid == 0) {
        close(s[0]);
        if (s[1] != STDOUT_FILENO) {
            dup2(s[1], STDOUT_FILENO);
            close(s[1]);
        }
        int i;
        for (i = 0; i <= MAX_WRITE_VALUE; ++i) {
            if (write(STDOUT_FILENO, &i, sizeof(i)) != sizeof(i)) {
                fprintf(stderr, "Failure to write i = %d\n", i);
                exit(EXIT_FAILURE);
            }
        }
        fprintf(stderr, "Subprocess quit.\n");
        exit(EXIT_SUCCESS);
    } else {
        pthread_t t;
        pthread_attr_t attr;
        child_harvest_params* params;

        close(s[1]);

        params = (child_harvest_params*) malloc(sizeof(child_harvest_params));
        params->pid = pid;
        params->fd = s[0];

        pthread_attr_init (&attr);
        pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&t, NULL, child_harvester_thread, params)) {
            close(s[0]);
            free(params);
            printf("Harvester thread not created.\n");
        }
    }

    return s[0];

}

int main(void) {

    int fd;
    ssize_t nread;
    int i;

    fd = get_fd();
    if (fd < 0) {
        printf("Failed to get fd.\n");
        return EXIT_FAILURE;
    }

    for (i = 0; i <= MAX_WRITE_VALUE; ++i) {
        int d;
        nread = read(fd, &d, sizeof(d));
        if (nread == 0) {
            break; // End of stream
        } else if (nread == sizeof(d)) {
            if (d != i) {
                printf("Failed sanity test: %d != %d.\n", d, i);
            }
        } else if (nread == -1) {
            printf("Read error: %d\n", errno);
            break;
        } else if (nread > 0) {
            printf("Short read.\n");
            break;
        }
#if FORCE_SMALL_DELAY
        pthread_yield();
#endif
#if FORCE_BIG_DELAY
        if (i % 10 == 0) {
            nanosleep(&BIG_DELAY, NULL);
        }
        //sleep(1);
#endif
    }

    if (i == MAX_WRITE_VALUE +1) {
        printf("All expected data was read.\n");
        return EXIT_SUCCESS;
    } else {
        printf("Too few data was read (%d)\n", i-1);
        return EXIT_FAILURE;
    }

}

