#ifndef E1000_H__
#define E1000_H__
/*
 * Driver for the E1000 Network Interface Controller
 * Controller information retrieved from the following sources:
 * 	https://wiki.osdev.org/Intel_8254x
 * 	www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf
 */
#include "types.h"
#include "nic.h"
#include "pci.h"

struct e1000;

int inite1000(pcifunc * pcif, void ** driver, uint8_t * mac);

void sende1000(void * e1000, uint8_t * pkt, uint16_t len);
void recve1000(void * e1000, uint8_t * pkt, uint16_t len);
#endif