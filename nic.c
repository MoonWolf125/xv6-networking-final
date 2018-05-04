#include "nic.h"
#include "defs.h"

void regnicdevice(nic d) { 
    nics[0] = d;
    cprintf("regnicdevice");
}

int getnicdevice(char * intrfc, nic ** d) {
    cprintf("Get device for interface=%s\n", intrfc);
    // TODO: Fetch device details from a table of loaded devices using interface name
    if (nics[0].sendpacket == 0 || nics[0].recvpacket == 0) {
	cprintf("NICS sendpacket:%d\n", nics[0].sendpacket);
	cprintf("NICS recvpacket:%d\n", nics[0].recvpacket);
	cprintf("ERROR: nic: No nic recognized\n");
	return -1;
    }
    * d = &nics[0];
    return 0;
}