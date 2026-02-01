#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

ssize_t write_all(int fd, const void* buf, size_t n);

int producer_run(int out_fd, uint32_t producer_id, const config_t* cfg) {
    // total bytes per message written in ONE call (header + payload)
    size_t msg_bytes = sizeof(msg_hdr_t) + cfg->msg_size;

    unsigned char* msgbuf = (unsigned char*)malloc(msg_bytes);
    if (!msgbuf) return 1;

    // payload starts right after header
    unsigned char* payload = msgbuf + sizeof(msg_hdr_t);
    memset(payload, 'A' + (producer_id % 26), cfg->msg_size);

    msg_hdr_t hdr;
    hdr.producer_id = producer_id;
    hdr.payload_len = cfg->msg_size;
    hdr.crc32 = 0;

    for (uint32_t i = 0; i < cfg->messages_per_producer; i++) {
        hdr.seq = i;

        // copy header into the front of msgbuf
        memcpy(msgbuf, &hdr, sizeof(hdr));

        // atomic message write (header+payload together)
        if (write_all(out_fd, msgbuf, msg_bytes) < 0) {
            perror("producer write message");
            free(msgbuf);
            return 2;
        }
    }

    free(msgbuf);
    return 0;
}

