#include "nic.h"
#include "defs.h"

void regnicdevice(struct nic d) { nics[0] = d; }

int getnicdevice(char * intrfc, struct nic ** d) {
    cprintf("Get device for interface=%s\n", intrfc);
    // TODO: Fetch device details from a table of loaded devices using interface name
    if (nics[0].sendpacket == 0 || nics[0].recvpacket == 0)
	return -1;
    * d = &nics[0];
    return 0;
}