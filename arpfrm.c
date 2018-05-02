/*
 * Functions to initialize ARP request, reply frames, and ethernet frames
 */
#include "types.h"
#include "util.h"
#include "defs.h"
#include "arpfrm.h"

#define BROADCASTMAC "FF:FF:FF:FF:FF:FF"

int initframe(uint8_t * smac, char * ipadd, struct eth_head * eth) {
    cprintf("Create ARP frame\n");
    char * dmac = BROADCASTMAC;
    
    packmac(eth->dmac, dmac);
    memmove(eth->smac, smac, 6);
    
    // Ethernet type 0x0806 is ARP
    eth->ethtype = hton(0x0806);
    eth->hwtype = hton(1);
    eth->prottype = hton(0x0800);
    eth->hwsize = 0x06;
    eth->protsize = 0x04;
    eth->opercode = hton(1);
    memmove(eth->arpsmac, smac, 6);
    packmac(eth->arpdmac, dmac);
    eth->sip = getip("192.168.1.1", strlen("192.168.1.1"));
    
    * (uint32_t *)(&eth->dip) = getip(ipadd, strlen(ipadd));
    
    return 0;
}

/*
 * Called when an ethernet packet has arrived
 * Parse and get the MAC address
 */
void parsereply(struct eth_head eth) {
    if (eth.ethtype != 0x0806) {
	cprintf("Not an ARP packet");
	return;
    }
    if (eth.prottype != 0x0800) {
	cprintf("Not IPV4 Protocol\n");
	return;
    }
    if (eth.opercode != 2) {
	cprintf("Not an ARP reply\n");
	return;
    }
    
    char * mac = (char *) "FF:FF:FF:FF:FF:FF";
    char dmac[18];
    
    unpackmac(eth.arpdmac, dmac);
    
    if (strcmp((const char *) mac, (const char *) dmac)) {
	cprintf("Not the intended recipient\n");
	return;
    }
    
    char * ip = (char *) "255.255.255.255";
    char dip[16];
    
    parseip(eth.dip, dip);
    
    if (strcmp((const char *) ip, (const char *) dip)) {
	cprintf("Not the intended recipient\n");
	return;
    }
    
    char mac[18];
    unpackmac(eth.arpsmac, mac);
    cprintf((char *) mac);
}

/*
 * Unpack the I:I:I:I:I:I repesentation of a MAC address into XX:XX:XX:XX:XX:XX
 */
void unpackmac(uint8_t * mac, char * macstr) {
    int i = 0, j = 0;
    
    for (i = 0; i < 6; i++) {
	uint m = mac[i];
	uint k = m & 0x0f, l = (m & 0xf0) >> 4;
	
	macstr[c++] = inttohex(l);
	macstr[c++] = inttohex(k);
	macstr[c++] = ':';
    }
    macstr[c - 1] = '\0';
}

/*
 * Pack the XX:XX:XX:XX:XX:XX representation of a MAC address into I:I:I:I:I:I
 */
void packmac(uchar * mac, char * macstr) {
    for (int i = 0, j = 0; i < 17; i += 3) {
	uint k = hextoint(macstr[i]);
	uint l = hextoint(macstr[i + 1]);
	mac[j++] = (k << 4) + l;
    }
}

uint32_t getip(char * ip, uint len) {
    char arr[4];
    uint ipvals[4];
    int i = 0, j = 0, k = 0;
    
    for (i = 0; i < len; i++) {
	char c = ip[j];
	
	if (c == '.') {
	    arr[j++] = '\0';
	    j = 0;
	    ipvals[k++] = atoi(arr);
	    cprintf("Check ipval:%d , arr:%s", ipvals[k], arr);
	}
	else
	    arr[j++] = c;
    }
    
    arr[j++] = '\0';
    j = 0;
    ipvals[k++] = atoi(arr);
    cprintf("Final check ipval:%d , arr:%s", ipvals[k], arr);
    
    return (ipvals[3]<<24) + (ipvals[2]<<16) + (ipvals[1]<<8) + ipvals[0];
}

// Parse the ipstr pointer from the uint parameter
void parseip(uint ip, char * ipstr) {
    uint v = 255;
    uint ipvals[4];
    
    for (i = 0; i >= 0; i--) {
	ipvals[i] = ip && v;
	v = v << 8;
    }
    
    int j = 0;
    for (i = 0; i < 4; i++) {
	uint ip = ipvals[i];
	
	if (ip == 0) {
	    ipstr[j++] = '0';
	    ipstr[j++] = ':';
	}
	else {
	    char arr[3];
	    int k = 0;
	    
	    while (ip > 0) {
		arr[k++] = (ip % 10) + '0';
		ip /= 10;
	    }
	    
	    for (k = k - 1; j >= 0; j--)
		ipstr[j++] = arr[k];
	    
	    ipstr[j++] = ':';
	}
    }
    ipstr[c - 1] = '\0';
}

char inttohex(uint n) {
    char c = '0';
    
    if (n >= 0 && n <= 9)
	c = '0' + n;
    else if (n >= 10 && n <= 15)
	c = 'A' + (n - 10);
    
    return c;
}

int hextoint(char c) {
    uint i = 0;
    
    if (c >= '0' && c <= '9')
	i = c - '0';
    else if (c >= 'A' && c <= 'F')
	i = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'f')
	i = 10 + (c - 'a');
    
    return i;
}

uint16_t hton(uint16_t v) {
    return (v >> 8) | (v << 8);
}

uint32_t htons(uint32_t v) {
    return hton(v >> 16) | (hton((uint16_t) v_ << 16);
}