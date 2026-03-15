#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <mqueue.h>

#define MAX_PAYLOAD 512
#define SENTINEL_PRODUCER_ID 0xFFFFFFFFu

typedef struct {
    uint64_t total_received;
    uint64_t duplicates;
    uint64_t out_of_range;
    uint64_t malformed;
} stats_t;

typedef struct {
    msg_hdr_t hdr;
    unsigned char payload[MAX_PAYLOAD];
} mq_msg_t;

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--producers N] [--consumers N] [--messages M] [--msg-size BYTES] [--maxmsg N] [--verbose]\n"
        "Example:\n"
        "  %s --producers 2 --consumers 2 --messages 10000 --msg-size 64 --maxmsg 64\n",
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
    return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

static int producer_run(mqd_t q, uint32_t producer_id, const config_t* cfg) {
    mq_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.hdr.producer_id = producer_id;
    msg.hdr.payload_len = cfg->msg_size;
    msg.hdr.crc32 = 0;

    memset(msg.payload, 'A' + (producer_id % 26), cfg->msg_size);

    for (uint32_t i = 0; i < cfg->messages_per_producer; i++) {
        msg.hdr.seq = i;
        if (mq_send(q, (const char*)&msg, sizeof(msg_hdr_t) + cfg->msg_size, 0) < 0) {
            perror("mq_send");
            return 1;
        }
    }
    return 0;
}

static int consumer_run(mqd_t q, const config_t* cfg, stats_t* out) {
    stats_t st = {0};

    size_t P = (size_t)cfg->producers;
    size_t M = (size_t)cfg->messages_per_producer;
    size_t seen_sz = P * M;
    unsigned char* seen = (unsigned char*)calloc(seen_sz, 1);
    if (!seen) return 1;

    mq_msg_t msg;
    while (1) {
        ssize_t r = mq_receive(q, (char*)&msg, sizeof(msg), NULL);
        if (r < 0) {
            perror("mq_receive");
            free(seen);
            return 2;
        }

        // sentinel to stop
        if (msg.hdr.producer_id == SENTINEL_PRODUCER_ID) break;

        if (msg.hdr.payload_len != cfg->msg_size) {
            st.malformed++;
            continue;
        }

        st.total_received++;

        if (msg.hdr.producer_id >= (uint32_t)cfg->producers || msg.hdr.seq >= cfg->messages_per_producer) {
            st.out_of_range++;
            continue;
        }

        size_t idx = (size_t)msg.hdr.producer_id * M + (size_t)msg.hdr.seq;
        if (seen[idx]) st.duplicates++;
        else seen[idx] = 1;
    }

    free(seen);
    *out = st;
    return 0;
}

int main(int argc, char** argv) {
    config_t cfg = {
        .producers = DEFAULT_PRODUCERS,
        .consumers = DEFAULT_CONSUMERS,
        .messages_per_producer = DEFAULT_MESSAGES_PER_PRODUCER,
        .msg_size = DEFAULT_MSG_SIZE,
        .verbose = 0
    };
    int maxmsg = 10;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--producers") && i + 1 < argc) cfg.producers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--consumers") && i + 1 < argc) cfg.consumers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--messages") && i + 1 < argc) cfg.messages_per_producer = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--msg-size") && i + 1 < argc) cfg.msg_size = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--maxmsg") && i + 1 < argc) maxmsg = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) cfg.verbose = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 1; }
    }

    if (cfg.producers <= 0 || cfg.consumers <= 0 || cfg.messages_per_producer == 0 || cfg.msg_size == 0) {
        fprintf(stderr, "Error: invalid parameters.\n");
        return 2;
    }
    if (cfg.msg_size > MAX_PAYLOAD) {
        fprintf(stderr, "Error: --msg-size must be <= %d.\n", MAX_PAYLOAD);
        return 2;
    }
    if (maxmsg <= 0) {
        fprintf(stderr, "Error: --maxmsg must be > 0.\n");
        return 2;
    }
    // Many Linux systems default msg_max to 10 (/proc/sys/fs/mqueue/msg_max).
    // If maxmsg is too large, mq_open() can fail with EINVAL.
    if (maxmsg > 10) {
    fprintf(stderr, "Note: --maxmsg=%d is above typical Linux default; using 10 instead.\n", maxmsg);
    maxmsg = 10;
}

    // Unique queue name
    char qname[128];
    snprintf(qname, sizeof(qname), "/cs4800_mq_%ld", (long)getpid());

    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = maxmsg;
    attr.mq_msgsize = sizeof(msg_hdr_t) + cfg.msg_size;

    mqd_t q = mq_open(qname, O_CREAT | O_EXCL | O_RDWR, 0600, &attr);
    if (q == (mqd_t)-1) {
        perror("mq_open");
        return 3;
    }

    if (cfg.verbose) {
        fprintf(stderr, "mq_name=%s maxmsg=%d msgsize=%ld\n", qname, maxmsg, (long)attr.mq_msgsize);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // Fork consumers first
    for (int c = 0; c < cfg.consumers; c++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork consumer"); return 4; }
        if (pid == 0) {
            stats_t st = {0};
            int rc = consumer_run(q, &cfg, &st);
            printf("consumer[%d]: received=%llu dup=%llu out_of_range=%llu malformed=%llu\n",
                   c,
                   (unsigned long long)st.total_received,
                   (unsigned long long)st.duplicates,
                   (unsigned long long)st.out_of_range,
                   (unsigned long long)st.malformed);
            _exit(rc);
        }
    }

    // Fork producers
    for (int p = 0; p < cfg.producers; p++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork producer"); return 5; }
        if (pid == 0) {
            int rc = producer_run(q, (uint32_t)p, &cfg);
            _exit(rc);
        }
    }

    // Wait for producers to finish (consumers should still be running)
    int status = 0;
    int child_error = 0;
    for (int i = 0; i < cfg.producers; i++) {
        pid_t w = wait(&status);
        if (w < 0) { perror("wait"); child_error = 1; break; }
        if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) child_error = 1;
    }

    // Send sentinels (one per consumer)
    mq_msg_t sentinel;
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.hdr.producer_id = SENTINEL_PRODUCER_ID;
    sentinel.hdr.seq = 0;
    sentinel.hdr.payload_len = cfg.msg_size;

    for (int i = 0; i < cfg.consumers; i++) {
        if (mq_send(q, (const char*)&sentinel, sizeof(msg_hdr_t) + cfg.msg_size, 0) < 0) {
            perror("mq_send sentinel");
            child_error = 1;
        }
    }

    // Reap consumers
    for (int i = 0; i < cfg.consumers; i++) {
        pid_t w = wait(&status);
        if (w < 0) { perror("wait consumer"); child_error = 1; break; }
        if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) child_error = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = elapsed_sec(t0, t1);

    unsigned long long total_msgs =
        (unsigned long long)cfg.producers * (unsigned long long)cfg.messages_per_producer;

    double msgs_per_sec = (sec > 0.0) ? ((double)total_msgs / sec) : 0.0;

    printf("run(mq): producers=%d consumers=%d messages_per_producer=%u msg_size=%u maxmsg=%d\n",
           cfg.producers, cfg.consumers, cfg.messages_per_producer, cfg.msg_size, maxmsg);
    printf("timing: %.3f sec | approx %.0f msgs/sec\n", sec, msgs_per_sec);

    mq_close(q);
    mq_unlink(qname);

    return child_error ? 6 : 0;
}