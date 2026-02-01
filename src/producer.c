#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ssize_t write_all(int fd, const void* buf, size_t n);

int producer_run(int out_fd, uint32_t producer_id, const config_t* cfg) {
    // Build one reusable payload
    char* payload = (char*)malloc(cfg->msg_size);
    if (!payload) return 1;
    memset(payload, 'A' + (producer_id % 26), cfg->msg_size);

    msg_hdr_t hdr;
    hdr.producer_id = producer_id;
    hdr.payload_len = cfg->msg_size;
    hdr.crc32 = 0;

    for (uint32_t i = 0; i < cfg->messages_per_producer; i++) {
        hdr.seq = i;

        if (write_all(out_fd, &hdr, sizeof(hdr)) < 0) {
            perror("producer write header");
            free(payload);
            return 2;
        }
        if (write_all(out_fd, payload, cfg->msg_size) < 0) {
            perror("producer write payload");
            free(payload);
            return 3;
        }
    }

    free(payload);
    return 0;
}

