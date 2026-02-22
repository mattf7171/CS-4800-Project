#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>

#define MAX_SLOTS 1024
#define MAX_PAYLOAD 512
#define SENTINEL_PRODUCER_ID 0xFFFFFFFFu

typedef struct {
    msg_hdr_t hdr;
    unsigned char payload[MAX_PAYLOAD];
} shm_msg_t;

typedef struct {
    sem_t empty;
    sem_t full;
    sem_t mutex;

    uint32_t write_idx;
    uint32_t read_idx;

    uint32_t slots;      // configured ring size
    uint32_t msg_size;   // configured payload size

    // Ring buffer
    shm_msg_t ring[MAX_SLOTS];
} shm_region_t;

typedef struct {
    uint64_t total_received;
    uint64_t duplicates;
    uint64_t out_of_range;
    uint64_t malformed;
} stats_t;

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--producers N] [--consumers N] [--messages M] [--msg-size BYTES] [--slots N] [--verbose]\n"
        "Example:\n"
        "  %s --producers 2 --consumers 2 --messages 10000 --msg-size 64 --slots 64\n",
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

static int queue_push(shm_region_t* shm, const shm_msg_t* msg) {
    if (sem_wait(&shm->empty) < 0) return -1;
    if (sem_wait(&shm->mutex) < 0) return -1;

    shm->ring[shm->write_idx] = *msg;
    shm->write_idx = (shm->write_idx + 1) % shm->slots;

    if (sem_post(&shm->mutex) < 0) return -1;
    if (sem_post(&shm->full) < 0) return -1;
    return 0;
}

static int queue_pop(shm_region_t* shm, shm_msg_t* msg_out) {
    if (sem_wait(&shm->full) < 0) return -1;
    if (sem_wait(&shm->mutex) < 0) return -1;

    *msg_out = shm->ring[shm->read_idx];
    shm->read_idx = (shm->read_idx + 1) % shm->slots;

    if (sem_post(&shm->mutex) < 0) return -1;
    if (sem_post(&shm->empty) < 0) return -1;
    return 0;
}

static int producer_run(shm_region_t* shm, uint32_t producer_id, const config_t* cfg) {
    shm_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.hdr.producer_id = producer_id;
    msg.hdr.payload_len = cfg->msg_size;
    msg.hdr.crc32 = 0;
    memset(msg.payload, 'A' + (producer_id % 26), cfg->msg_size);

    for (uint32_t i = 0; i < cfg->messages_per_producer; i++) {
        msg.hdr.seq = i;
        if (queue_push(shm, &msg) < 0) {
            perror("queue_push (producer)");
            return 1;
        }
    }
    return 0;
}

static int consumer_run(shm_region_t* shm, const config_t* cfg, stats_t* st_out) {
    stats_t st = {0};

    size_t P = (size_t)cfg->producers;
    size_t M = (size_t)cfg->messages_per_producer;
    size_t seen_sz = P * M;
    unsigned char* seen = (unsigned char*)calloc(seen_sz, 1);
    if (!seen) return 1;

    shm_msg_t msg;
    while (1) {
        if (queue_pop(shm, &msg) < 0) {
            perror("queue_pop (consumer)");
            free(seen);
            return 2;
        }

        if (msg.hdr.producer_id == SENTINEL_PRODUCER_ID) {
            break; // graceful shutdown marker
        }

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
    *st_out = st;
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
    int slots = 64;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--producers") && i + 1 < argc) cfg.producers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--consumers") && i + 1 < argc) cfg.consumers = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--messages") && i + 1 < argc) cfg.messages_per_producer = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--msg-size") && i + 1 < argc) cfg.msg_size = (uint32_t)parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--slots") && i + 1 < argc) slots = parse_int(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) cfg.verbose = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 1; }
    }

    if (cfg.producers <= 0 || cfg.consumers <= 0 || cfg.messages_per_producer == 0 || cfg.msg_size == 0) {
        fprintf(stderr, "Error: invalid parameters.\n");
        return 2;
    }
    if (slots <= 0 || slots > MAX_SLOTS) {
        fprintf(stderr, "Error: --slots must be between 1 and %d.\n", MAX_SLOTS);
        return 2;
    }
    if (cfg.msg_size > MAX_PAYLOAD) {
        fprintf(stderr, "Error: --msg-size must be <= %d for shared-memory ring slots.\n", MAX_PAYLOAD);
        return 2;
    }

    // Create unique shm object name
    char shm_name[128];
    snprintf(shm_name, sizeof(shm_name), "/cs4800_shm_%ld", (long)getpid());

    int fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return 3;
    }

    if (ftruncate(fd, sizeof(shm_region_t)) < 0) {
        perror("ftruncate");
        shm_unlink(shm_name);
        close(fd);
        return 4;
    }

    shm_region_t* shm = (shm_region_t*)mmap(NULL, sizeof(shm_region_t),
                                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        shm_unlink(shm_name);
        close(fd);
        return 5;
    }
    close(fd);

    memset(shm, 0, sizeof(*shm));
    shm->slots = (uint32_t)slots;
    shm->msg_size = cfg.msg_size;

    if (sem_init(&shm->empty, 1, (unsigned int)slots) < 0 ||
        sem_init(&shm->full, 1, 0) < 0 ||
        sem_init(&shm->mutex, 1, 1) < 0) {
        perror("sem_init");
        munmap(shm, sizeof(*shm));
        shm_unlink(shm_name);
        return 6;
    }

    if (cfg.verbose) {
        fprintf(stderr, "shm_name=%s slots=%d msg_size=%u\n", shm_name, slots, cfg.msg_size);
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // Fork consumers
    for (int c = 0; c < cfg.consumers; c++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork consumer");
            return 7;
        }
        if (pid == 0) {
            stats_t st = {0};
            int rc = consumer_run(shm, &cfg, &st);
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
        if (pid < 0) {
            perror("fork producer");
            return 8;
        }
        if (pid == 0) {
            int rc = producer_run(shm, (uint32_t)p, &cfg);
            _exit(rc);
        }
    }

    // Wait for producers first
    int status = 0;
    int producers_done = 0;
    int child_error = 0;
    int total_children = cfg.producers + cfg.consumers;
    int reaped = 0;

    while (producers_done < cfg.producers) {
        pid_t w = wait(&status);
        if (w < 0) {
            if (errno == EINTR) continue;
            perror("wait");
            child_error = 1;
            break;
        }
        reaped++;
        producers_done++; // we don't distinguish here; good enough if producers were forked last? no
        // Correction: this loop assumes producer-only waits, but consumers may exit later only after sentinel.
        // Consumers should still be blocked, so the first cfg.producers exits should all be producers.
        if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) child_error = 1;
    }

    // Send one sentinel per consumer
    shm_msg_t sentinel;
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.hdr.producer_id = SENTINEL_PRODUCER_ID;
    sentinel.hdr.seq = 0;
    sentinel.hdr.payload_len = cfg.msg_size;

    for (int i = 0; i < cfg.consumers; i++) {
        if (queue_push(shm, &sentinel) < 0) {
            perror("queue_push sentinel");
            child_error = 1;
        }
    }

    // Reap remaining consumers
    while (reaped < total_children) {
        pid_t w = wait(&status);
        if (w < 0) {
            if (errno == EINTR) continue;
            break;
        }
        reaped++;
        if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) child_error = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = elapsed_sec(t0, t1);

    unsigned long long total_msgs =
        (unsigned long long)cfg.producers * (unsigned long long)cfg.messages_per_producer;
    double msgs_per_sec = (sec > 0.0) ? ((double)total_msgs / sec) : 0.0;

    printf("run(shm_sem): producers=%d consumers=%d messages_per_producer=%u msg_size=%u slots=%d\n",
           cfg.producers, cfg.consumers, cfg.messages_per_producer, cfg.msg_size, slots);
    printf("timing: %.3f sec | approx %.0f msgs/sec\n", sec, msgs_per_sec);

    sem_destroy(&shm->empty);
    sem_destroy(&shm->full);
    sem_destroy(&shm->mutex);
    munmap(shm, sizeof(*shm));
    shm_unlink(shm_name);

    return child_error ? 9 : 0;
}