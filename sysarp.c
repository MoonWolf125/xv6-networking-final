 /*
  * System Call to initiate and send an ARP request
  */
#include "types.h"
#include "defs.h"

int sys_arp(void) {
    char * ipadd, * intrfc, * arpresp;
    int size;
    
    if (argstr(0, &intrfc) < 0 || argstr(1, &ipadd) < 0 || argint(3, &size) < 0 || argptr(2, &arpresp, size) < 0) {
	cprintf("ERROR: sysarp: Failed to get Args\n");
	return -1;
    }
    if (sendrequest(intrfc, ipadd, arpresp) < 0) {
	cprintf("ERROR: sysarp: Failed to send ARP request for IP: %s\n", ipadd);
	return -1;
    }
    return 0;
}