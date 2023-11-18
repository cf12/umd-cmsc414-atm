#ifndef __PACKET_H__
#define __PACKET_H__

typedef enum { BeginSession } Command;
typedef struct {
    Command cmd;
    char username[250];
    char card[32];
    unsigned int pin;
    unsigned int nonce;
} packet_t;

#endif