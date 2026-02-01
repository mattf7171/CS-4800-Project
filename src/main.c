#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

// Forward declarations
int producer_run(int out_fd, uint32_t producer_id, const config_t* cfg);

// Must match the struct used in consumer.c
typedef struct {
    uint64_t total_received;
    uint64_t duplicates;
    uint64_t out_of_range;
    uint64_t malformed;
} stats_t;

int consumer_run(int in_fd, const config_t* cfg, stats_t* stats_out);

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--producers N] [--consumers N] [--messages M] [--msg-size BYTES] [--verbose]\n"
        "\n"
        "Example:\n"
        "  %s --producers 4 --consumers 1 --messages 5000 --msg-size 64\n",
        prog, prog
    );
}

static int parse_int(const char* s) {
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0') return -1;
    if (v < 0 || v > 1000000000L) return -1;
    return (int)v;
}

static double elapsed_sec(struct timespec a, struct timespec b) {
    double sec = (double)(b.tv_sec - a.tv_sec);
    double nsec = (double)(b.tv_nsec - a.tv_nsec) / 1e9;
    return sec + nsec;
}

int main(int argc, char** argv) {
    config_t cfg = {
        .producers = DEFAULT_PRODUCERS,
        .consumers = DEFAULT_CONSUMERS,
        .messages_per_producer = DEFAULT_MESSAGES_PER_PRODUCER,
        .msg_size = DEFAULT_MSG_SIZE,
        .verbose = 0
    };

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--producers") && i + 1 < argc) {
            cfg.producers = parse_int(argv[++i]);
        } else if (!strcmp(argv[i], "--consumers") && i + 1 < argc) {
            cfg.consumers = parse_int(argv[++i]);
        } else if (!strcmp(argv[i], "--messages") && i + 1 < argc) {
            cfg.messages_per_producer = (uint32_t)parse_int(argv[++i]);
        } else if (!strcmp(argv[i], "--msg-size") && i + 1 < argc) {
            cfg.msg_size = (uint32_t)parse_int(argv[++i]);
        } else if (!strcmp(argv[i], "--verbose")) {
            cfg.verbose = 1;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (cfg.producers <= 0 || cfg.consumers <= 0 || cfg.messages_per_producer == 0 || cfg.msg_size == 0) {
        fprintf(stderr, "Error: invalid parameters.\n");
        usage(argv[0]);
        return 2;
    }

    // PIPE_BUF guard: ensure each message write is atomic for multi-producer pipe usage
    size_t msg_bytes = sizeof(msg_hdr_t) + (size_t)cfg.msg_size;
    if (msg_bytes > (size_t)PIPE_BUF) {
        fprintf(stderr,
            "Error: message size (%zu) exceeds PIPE_BUF (%d). Reduce --msg-size.\n",
            msg_bytes, PIPE_BUF
        );
        return 2;
    }

    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return 3;
    }

    if (cfg.verbose) {
        fprintf(stderr, "PIPE_BUF=%d, msg_bytes=%zu\n", PIPE_BUF, msg_bytes);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // Fork consumers (read end)
    for (int c = 0; c < cfg.consumers; c++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork (consumer)");
            return 4;
        }
        if (pid == 0) {
            // child consumer
            close(pipefd[1]); // close write end

            stats_t st = {0};
            int rc = consumer_run(pipefd[0], &cfg, &st);

            // Print per-consumer stats (nice evidence)
            printf("consumer[%d]: received=%llu dup=%llu out_of_range=%llu malformed=%llu\n",
                   c,
                   (unsigned long long)st.total_received,
                   (unsigned long long)st.duplicates,
                   (unsigned long long)st.out_of_range,
                   (unsigned long long)st.malformed);

            close(pipefd[0]);
            _exit(rc);
        }
    }

    // Fork producers (write end)
    for (int p = 0; p < cfg.producers; p++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork (producer)");
            return 5;
        }
        if (pid == 0) {
            // child producer
            close(pipefd[0]); // close read end

            int rc = producer_run(pipefd[1], (uint32_t)p, &cfg);

            close(pipefd[1]);
            _exit(rc);
        }
    }

    // Parent: close both ends so consumers get EOF when producers exit
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for all children
    int status = 0;
    int child_rc_nonzero = 0;
    while (1) {
        pid_t w = wait(&status);
        if (w < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            child_rc_nonzero = 1;
        } else if (WIFSIGNALED(status)) {
            child_rc_nonzero = 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = elapsed_sec(t0, t1);

    unsigned long long total_msgs =
        (unsigned long long)cfg.producers * (unsigned long long)cfg.messages_per_producer;

    double msgs_per_sec = (sec > 0.0) ? ((double)total_msgs / sec) : 0.0;

    printf("run: producers=%d consumers=%d messages_per_producer=%u msg_size=%u\n",
           cfg.producers, cfg.consumers, cfg.messages_per_producer, cfg.msg_size);
    printf("timing: %.3f sec | approx %.0f msgs/sec\n", sec, msgs_per_sec);

    return child_rc_nonzero ? 6 : 0;
}

