#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ssize_t read_all(int fd, void* buf, size_t n);

typedef struct {
    uint64_t total_received;
    uint64_t duplicates;
    uint64_t out_of_range;
} stats_t;

int consumer_run(int in_fd, const config_t* cfg, stats_t* stats_out) {
    stats_t st = {0};
    // Track seen message IDs: producers x messages_per_producer (bitset-ish via byte array)
    size_t P = (size_t)cfg->producers;
    size_t M = (size_t)cfg->messages_per_producer;
    size_t seen_sz = P * M;
    unsigned char* seen = (unsigned char*)calloc(seen_sz, 1);
    if (!seen) return 1;

    msg_hdr_t hdr;
    char* payload = (char*)malloc(cfg->msg_size);
    if (!payload) { free(seen); return 2; }

    while (1) {
        ssize_t r = read_all(in_fd, &hdr, sizeof(hdr));
        if (r == 0) break;          // EOF
        if (r < 0) { perror("consumer read header"); break; }

        if (hdr.payload_len != cfg->msg_size) {
            fprintf(stderr, "consumer: unexpected payload_len=%u\n", hdr.payload_len);
            // still try to read to keep stream aligned
        }

        if (read_all(in_fd, payload, cfg->msg_size) <= 0) {
            perror("consumer read payload");
            break;
        }

        st.total_received++;

        if (hdr.producer_id >= (uint32_t)cfg->producers || hdr.seq >= cfg->messages_per_producer) {
            st.out_of_range++;
            continue;
        }

        size_t idx = (size_t)hdr.producer_id * M + (size_t)hdr.seq;
        if (seen[idx]) st.duplicates++;
        else seen[idx] = 1;
    }

    free(payload);
    free(seen);
    *stats_out = st;
    return 0;
}

