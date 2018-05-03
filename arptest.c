#include "types.h"
#include "user.h"

int main(void) {
    int MACSIZE = 18;
    char * ip = "192.168.2.1", mac = malloc(MACSIZE);
    if (arp("mynet0", ip, mac, MACSIZE) < 0)
	printf(1, "ARP for IP:%s Failed\n", ip);
    exit();
}