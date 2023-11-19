#ifndef __PACKET_H__
#define __PACKET_H__

typedef enum { BeginSession } Command;
typedef struct {
    Command cmd;
    char username[251];
    unsigned int card;
    unsigned int pin;
    unsigned int nonce;
} packet_t;

#endif