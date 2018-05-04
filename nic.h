#ifndef NIC_H__
#define NIC_H__
/*
 * Initialize device drivers for different Network Interface Controllers
 */
#include "types.h"
#include "arpfrm.h"

// Network Interface Device Driver Container
typedef struct {
    void * drvr;
    uint8_t macaddr[6];
    void (* sendpacket)(void * drvr, uint8_t * pkt, uint16_t len);
    void (* recvpacket)(void * drvr, uint8_t * pkt, uint16_t len);
} nic;

nic nics[1];

void regnicdevice(nic d);
int getnicdevice(char * intrfc, nic ** d);
#endif