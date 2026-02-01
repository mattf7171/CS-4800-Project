#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ssize_t read_all(int fd, void* buf, size_t n);

typedef struct {
    uint64_t total_received;
    uint64_t duplicates;
    uint64_t out_of_range;
    uint64_t malformed;
} stats_t;

int consumer_run(int in_fd, const config_t* cfg, stats_t* stats_out) {
    stats_t st = {0};

    size_t P = (size_t)cfg->producers;
    size_t M = (size_t)cfg->messages_per_producer;
    size_t seen_sz = P * M;
    unsigned char* seen = (unsigned char*)calloc(seen_sz, 1);
    if (!seen) return 1;

    size_t msg_bytes = sizeof(msg_hdr_t) + cfg->msg_size;
    unsigned char* msgbuf = (unsigned char*)malloc(msg_bytes);
    if (!msgbuf) { free(seen); return 2; }

    while (1) {
        ssize_t r = read_all(in_fd, msgbuf, msg_bytes);
        if (r == 0) break; // EOF
        if (r < 0) { perror("consumer read message"); break; }

        msg_hdr_t hdr;
        memcpy(&hdr, msgbuf, sizeof(hdr));

        if (hdr.payload_len != cfg->msg_size) {
            st.malformed++;
            continue;
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

    free(msgbuf);
    free(seen);
    *stats_out = st;
    return 0;
}

