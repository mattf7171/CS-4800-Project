#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

int producer_run(int out_fd, uint32_t producer_id, const config_t* cfg);
int consumer_run(int in_fd, const config_t* cfg, void* stats_out);

typedef struct {
    unsigned long long total_received;
    unsigned long long duplicates;
    unsigned long long out_of_range;
} stats_t;

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--producers N] [--consumers N] [--messages M] [--msg-size BYTES] [--verbose]\n",
        prog);
}

static int parse_int(const char* s) {
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0' || v < 0) return -1;
    return (int)v;
}

int main(int argc, char** argv) {
    config_t cfg = {
        .producers = DEFAULT_PRODUCERS,
        .consumers = DEFAULT_CONSUMERS,
        .messages_per_producer = DEFAULT_MESSAGES_PER_PRODUCER,
        .msg_size = DEFAULT_MSG_SIZE,
        .verbose = 0
    };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--producers") && i+1 < argc) cfg.producers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--consumers") && i+1 < argc) cfg.consumers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--messages") && i+1 < argc) cfg.messages_per_producer = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--msg-size") && i+1 < argc) cfg.msg_size = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) cfg.verbose = 1;
        else { usage(argv[0]); return 1; }
    }

    if (cfg.producers <= 0 || cfg.consumers <= 0 || cfg.messages_per_producer == 0 || cfg.msg_size == 0) {
        fprintf(stderr, "Invalid args.\n");
        return 2;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) { perror("pipe"); return 3; }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // Fork consumers (they read from pipefd[0])
    for (int c = 0; c < cfg.consumers; c++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[1]); // close write end
            stats_t st = {0};
            consumer_run(pipefd[0], &cfg, &st);
            // Print per-consumer stats (for evidence)
            fprintf(stdout, "consumer[%d]: received=%llu dup=%llu out_of_range=%llu\n",
                    c, st.total_received, st.duplicates, st.out_of_range);
            close(pipefd[0]);
            _exit(0);
        }
    }

    // Fork producers (they write to pipefd[1])
    for (int p = 0; p < cfg.producers; p++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]); // close read end
            int rc = producer_run(pipefd[1], (uint32_t)p, &cfg);
            close(pipefd[1]);
            _exit(rc);
        }
    }

    // Parent
    close(pipefd[0]);
    close(pipefd[1]); // important: allow consumers to see EOF when producers finish

    int status = 0;
    while (wait(&status) > 0) { /* collect children */ }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    unsigned long long total_msgs = (unsigned long long)cfg.producers * (unsigned long long)cfg.messages_per_producer;
    double msgs_per_sec = total_msgs / sec;

    printf("run: producers=%d consumers=%d messages_per_producer=%u msg_size=%u\n",
           cfg.producers, cfg.consumers, cfg.messages_per_producer, cfg.msg_size);
    printf("timing: %.3f sec | approx %.0f msgs/sec\n", sec, msgs_per_sec);

    return 0;
}

