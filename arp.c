/*
 * Kernel code to send and receive ARP requests and responses
 */
#include "types.h"
#include "defs.h"
#include "arpfrm.h"
#include "nic.h"

static int blockuntilreply(eth_head * eth) {
    /*
     * TODO: Sleep until woken up by network interrupt.
     * 		check for ARP reply with this request.
     * 		If reply is received unblock, otherwise sleep.
     */
    return 0;
}

int sendrequest(char * interface, char * ipadd, char * arpresp) {
    cprintf("Create ARP request for IP:%s over Interface:%s\n", ipadd, interface);
    
    // Test if the NIC is found/connected/loaded
    nic * _nic;
    // TODO
    if (getnicdevice(interface, &_nic) < 0) {
	cprintf("ERROR: sendrequest : Device not loaded\n");
	return -1;
    }
    
    // Create an ARP packet and send it across the NIC
    eth_head eth;
    initframe(_nic->macaddr, ipadd, &eth);
    _nic->sendpacket(_nic->drvr, (uint8_t *) &eth, sizeof(eth) - 2);	// Removing the padding
    
    // Initialize the ARP response and test if it exists
    eth_head resp;
    if (blockuntilreply(&resp) < 0) {
	cprintf("ERROR: sendrequest : Device not loaded\n");
	return -3;
    }
    
    unpackmac(resp.arpdmac, arpresp);
    arpresp[17] = '\0';
    
    return 0;
}