/*
 * Initialize device drivers for different Network Interface Controllers
 */
#include "types.h"
#include "arpfrm.h"

// Network Interface Device Driver Container
struct nic {
    void * drvr;
    uint8_t macaddr[6];
    void (* sendpacket)(void * drvr, uint8_t * pkt, uint16_t len);
    void (* recvpacket)(void * drvr, uint8_t * pkt, uint16_t len);
};

struct nic nics[1];

void regdevice(struct nic d);
int getdevice(char * intrfc, struct nic ** d);