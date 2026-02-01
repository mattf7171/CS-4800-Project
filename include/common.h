#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

#define DEFAULT_PRODUCERS 2
#define DEFAULT_CONSUMERS 2
#define DEFAULT_MESSAGES_PER_PRODUCER 10000
#define DEFAULT_MSG_SIZE 32

// Message format: fixed header + payload
typedef struct {
    uint32_t producer_id;
    uint32_t seq;        // sequence number for that producer
    uint32_t payload_len;
    uint32_t crc32;      // optional, can be 0 for now
    // payload follows (payload_len bytes)
} msg_hdr_t;

typedef struct {
    int producers;
    int consumers;
    uint32_t messages_per_producer;
    uint32_t msg_size;
    int verbose;
} config_t;

#endif

