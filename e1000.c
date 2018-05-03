/*
 * Driver for the E1000 Network Interface Controller
 * Comments and definitions are in the same order
 * Controller information retrieved from the following sources:
 * 	https://wiki.osdev.org/Intel_8254x
 * 	www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf
 * 	https://github.com/wh5a/jps/blob/master/kern/e100.c
 */
#include "e1000.h"
#include "defs.h"
#include "x86.h"
#include "arpfrm.h"
#include "nic.h"
#include "memlayout.h"

/*
 * Buffer descriptor slots
 * 	Receive
 * 	Send
 */
#define E1000_RBD_SLOTS 128
#define E1000_TBD_SLOTS 128

/*
 * IO Offsets
 * 	Address
 * 	Data
 */
#define E1000_IO_ADDR_OFFSET 0x00000000
#define E1000_IO_DATA_OFFSET 0x00000004

// 

/*
 * Ethernet Controller Register values
 * 	Control Register
 *	Controller reset (PCI Configuration Unaffected)
 * 	Auto-speed Detection Enable
 * 	Set Link Up
 */
#define E1000_CONTROL_REG		0x00000
#define E1000_CONTROL_RESET_MASK	0x04000000
#define E1000_CONTROL_ASDE_MASK		0x00000020
#define E1000_CONTROL_SLUP_MASK		0x00000040

#define E1000_CNTRL_RST_BIT(cntrl) \
        (cntrl & E1000_CNTRL_RST_MASK)

/*
 * Ethernet Device Registers
 * 	Receive Descriptor Base Address Low
 * 	Receive Descriptor Base Address High
 * 	Receive Descriptor Length
 * 	Receive Descriptor Head
 * 	Receive Descriptor Tail
 * 	Transmit Descriptor Base Address Low
 * 	Transmit Descriptor Base Address High
 * 	Transmit Descriptor Length
 * 	Transmit Descriptor Head
 * 	Transmit Descriptor Tail
 * 	Receive Address Low
 * 	Receive Address High
 */
#define E1000_RDBAL	0x02800
#define E1000_RDBAH	0x02804
#define E1000_RDLEN	0x02808
#define E1000_RDH	0x02810
#define E1000_RDT	0x02818
#define E1000_TDBAL	0x03800
#define E1000_TDBAH	0x03804
#define E1000_TDLEN	0x03808
#define E1000_TDH	0x03810
#define E1000_TDT	0x03818
#define E1000_RECV_RAL0	0x05400
#define E1000_RECV_RAH0	0x05404

/*
 * Ethernet Device Transmission Control Register
 * 	Enable
 * 	Pad Short Packets
 * 	Collision Threshold Bit Mask
 * 	Collision Threshold Bit Shift
 * 	Collision Distance Bit Mask
 * 	Collision Distance Bit Shift
 * 	Collision Threshold Set
 * 	Collision Distance Set
 */
#define E1000_TCTL			0x000400
#define E1000_TCTL_EN			0x00000002
#define E1000_TCTL_PSP			0x00000008
#define E1000_TCTL_CT_BIT_MASK		0x00000ff0
#define E1000_TCTL_CT_BIT_SHIFT		4
#define E1000_TCTL_COLD_BIT_MASK	0x003ff000
#define E1000_TCTL_COLD_BIT_SHIFT	12
#define E1000_TCTL_CT_SET(value) \
        ((value << E1000_TCTL_CT_BIT_SHIFT) & E1000_TCTL_CT_BIT_MASK)
#define E1000_TCTL_COLD_SET(value) \
        ((value << E1000_TCTL_COLD_BIT_SHIFT) & E1000_TCTL_COLD_BIT_MASK)

/*
 * Ethernet Device Transmission Inter-Packet Gap
 * 	Inter-Packet Gap Transmit Time Mask
 * 	Inter-Packet Gap Transmit Time Shift
 * 	Inter-Packet Gap Receive Time 1 Mask
 * 	Inter-Packet Gap Receive Time 1 Shift
 * 	Inter-Packet Gap Receive Time 2 Mask
 * 	Inter-Packet Gap Receive Time 2 Shift
 * 	Inter-Packet Gap Transmit Time Set
 * 	Inter-Packet Gap Receive Time 1 Set
 * 	Inter-Packet Gap Receive Time 2 Set
 */
#define E1000_TIPG			0x00410
#define E1000_TIPG_IPGT_BIT_MASK	0x000003ff
#define E1000_TIPG_IPGT_BIT_SHIFT	0
#define E1000_TIPG_IPGR1_BIT_MASK	0x000ffc00
#define E1000_TIPG_IPGR1_BIT_SHIFT	10
#define E1000_TIPG_IPGR2_BIT_MASK	0x3ff00000
#define E1000_TIPG_IPGR2_BIT_SHIFT	20
#define E1000_TIPG_IPGT_SET(value) \
        ((value << E1000_TIPG_IPGT_BIT_SHIFT) & E1000_TIPG_IPGT_BIT_MASK)
#define E1000_TIPG_IPGR1_SET(value) \
        ((value << E1000_TIPG_IPGR1_BIT_SHIFT) & E1000_TIPG_IPGR1_BIT_MASK)
#define E1000_TIPG_IPGR2_SET(value) \
        ((value << E1000_TIPG_IPGR2_BIT_SHIFT) & E1000_TIPG_IPGR2_BIT_MASK)

/*
 * Ethernet Device Transmit Descriptor Command
 * 	End Of Packet
 * 	Insert Frame Check Sequence
 * 	Report Status
 */
#define E1000_TDESC_CMD_EOP	0x01
#define E1000_TDESC_CMD_IFCS	0x02
#define E1000_TDESC_CMD_RS	0x08

/*
 * Ethernet Device Transmit Descriptor Status
 * 	Done Mask
 * 	Done Set
 */
#define E1000_TDESC_STATUS_DONE_MASK   0x01
#define E1000_TDESC_STATUS_DONE(status) \
        (status & E1000_TDESC_STATUS_DONE_MASK)

/*
 * Ethernet Device EEPROM Read
 * 	Register Address
 * 	Start Bit Mask
 * 	Done Bit Mask
 * 	Address Bit Mask
 * 	Address Bit Shift
 * 	Data Bit Mask
 * 	Data Bit Shift
 * 	Address Set
 * 	Data Set
 * 	Done Set
 */
#define E1000_EERD_REG_ADDR		0x00014
#define E1000_EERD_START_BIT_MASK	0x00000001
#define E1000_EERD_DONE_BIT_MASK	0x00000010
#define E1000_EERD_ADDR_BIT_MASK	0x0000ff00
#define E1000_EERD_ADDR_BIT_SHIFT	8
#define E1000_EERD_DATA_BIT_MASK	0xffff0000
#define E1000_EERD_DATA_BIT_SHIFT	16
#define E1000_EERD_ADDR(addr) \
        ((addr << E1000_EERD_ADDR_BIT_SHIFT) & E1000_EERD_ADDR_BIT_MASK)
#define E1000_EERD_DATA(eerd) \
        (eerd >> E1000_EERD_DATA_BIT_SHIFT)
#define E1000_EERD_DONE(eerd) \
        (eerd & E1000_EERD_DONE_BIT_MASK)

/*
 * EEPROM Address Map Ethernet Map Addresses
 */
#define EEPROM_ADDR_MAP_ETHER_ADDR_1	0x00
#define EEPROM_ADDR_MAP_ETHER_ADDR_2	0x01
#define EEPROM_ADDR_MAP_ETHER_ADDR_3	0x02

/*
 * Transmit Buffer Descriptor
 * 	Queue must be aligned on 16b boundary
 */
__attribute__ ((packed))
struct e1000_TBD {
    uint64_t addr;	// Buffer Address
    uint16_t len;	// Length
    uint8_t cso;	// Checksum Offset
    uint8_t cmd;	// Command
    uint8_t sts;	// Status
    uint8_t css;	// Checksum Start
    uint16_t spc;	// Special
};

/*
 * Receive Buffer Descriptor
 * 	Queue must be aligned on 16b boundary
 */
__attribute__ ((packed))
struct e1000_RBD {
    uint32_t addr0;	// First Buffer Address
    uint32_t addr1;	// Second Buffer Address
    uint16_t len;	// Length
    uint16_t csm;	// Checksum
    uint8_t sts;	// Status
    uint8_t err;	// Errors
    uint16_t spc;	// Special
};

struct packbuf {
    uint8_t buf[2046];
};

/*
 * E1000 Structure
 */
struct e1000 {
    // Transmit and Receive Buffer Descriptors
    struct e1000_TBD * tbd[E1000_TBD_SLOTS];
    struct e1000_RBD * rbd[E1000_RBD_SLOTS];
    
    // Packet buffer space for Transmit and Receieve
    struct packbuf * tbuf[E1000_TBD_SLOTS];
    struct packbuf * rbuf[E1000_RBD_SLOTS];
    
    int tbdhead, tbdtail, rbdhead, rbdtail;
    char tbdidle, rbdidle;
    uint32_t iobase;
    uint32_t membase;
    uint8_t irqline;	// Interrupt Request Line
    uint8_t irqpin;	// Interrupt Request Pin
    uint8_t mac[6];
};

static void e1000regwrite(uint32_t regaddr, uint32_t val, struct e1000 * e1000) {
    * (uint32_t *)(e1000->membase + regaddr) = value;
}

static uint32_t e1000regread(uint32_t regaddr, struct e1000 * e1000) {
    uint32_t val = * (uint32_t *)(e1000->membase + regaddr);
    cprintf("Read val 0x%x from E1000 IO port 0x%x\n", val, regaddr);
    return val;
}

static void delay(unsigned int u) {
    unsigned int i;
    for (i = 0; i < u; i++)
	inb(0x84);
}

void sende1000(void * drv, uint8_t * pkt, uint16_t len) {
    struct e1000 * e1000 = (struct e1000 *)drv;
    cprintf("E1000 driver: Sending packet of length: 0x%x %x starting at physical address: 0x%x\n", len, sizeof(struct ethhead), V2P(e1000->tbuf[e1000->tbdtail]));
    memset(e1000->tbd(e1000->tbdtail], 0, sizeof(struct e1000tbd));
    memmove((e1000->tbuf[e1000->tbdtail]), pkt, len);
    e1000->tbd[e1000->tbdtail]->addr = (uint64_t)(uint32_t)V2P(e1000->tbuf[e1000->tbdtail]);
    e1000->tbd[e1000->tbdtail]->len = len;
    e1000->tbd[e1000->tbdtail]->cmd = (E1000_TDESC_CMD_RS | E1000_TDESC_CMD_EOP | E1000_TDESC_CMD_IFCS);
    e1000->tbd[e1000->tbdtail]->cso = 0;
    int oldtail = e1000->tbdtail;
    e1000->tbdtail = (e1000->tbdtail + 1) % E1000_TBD_SLOTS;
    e1000redwrite(E1000_TDT, e1000->tbdtail, e1000);
    
    while (!E1000_TDESC_STATUS_DONE(e1000->tbd[oldtail]->sts)) {
	delay(2);
    }
    cprintf("after while loop\n");
}

int e1000init(struct pcifund * pcif, void ** drv, uint8_t * macaddr) {
    struct e1000 * e1000 = (struct e1000 *)kalloc();
    int i;
    for (i = 0, i < 6; i = i + 1) {
	if (pcif->regbase[i] <= 0xffff) {
	    e1000->iobase = pcif->regbase[i];
	    if (pcif->regsize[i] != 64) {
		panic("I/O space BAR size != 64");
	    }
	    break;
	}
	else if (pcif->regbase[i] > 0) {
	    e1000->membase = pcif->regbase[i];
	    if (pcif->regsize[i] != (1<<17))
		panic("Mem space BAR size != 128KB");
	}
    }
    if (!e1000->iobase) {
	panic("Failed to find a valid I/O port base for the E1000");
	if (!e1000->membase)
	    panic("Fail to find a valid Mem I/O base for E1000");
    }
    e1000->irqline = pcif->irqline;
    e1000->irqpin = pcif->irqpin;
    cprintf("E1000 init: Interrupt pin=%d and line:%d\n", e1000->irqpine, e1000->irqline);
    e1000->tbdhead = e1000->tbdtail = 0;
    e1000->rbdhead = e1000->rbdtail = 0;
    
    // Reset the device
    e1000regwrite(E1000_CNTRL_REG,
		  e1000regread(E1000_CNTRL_REG, e1000) | E1000_CNTRL_RST_MASK, e1000);
    do {
	delay(3);
    } while (E1000_CNTRL_RST_BIT(e1000regread(E1000_CNTRL_REG, e1000)));
    
    uint32_t cntrlreg = e1000regread(E1000_CNTRL_REG, e1000);
    e1000regwrite(E1000_CNTRL_REG, cntrlreg | E1000_CNTRL_ASDE_MASK | E1000_CNTRL_SLU_MASK, e1000);
    
    uint32_t macaddrl = e1000regread(E1000_RCV_RAL0, e1000);
    uint32_t macaddrh = e1000regread(E1000_RCV_RAH0, e1000);
    * (uint32_t *)e1000->macaddr = macaddrl;
    * (uint16_t *)(&e1000->macaddr[4]) = (uint16_t)macaddrh;
    * (uint32_t *)macaddr = macaddrl;
    * (uint32_t *)(&macaddr[4]) = (uint16_t)macaddrh;
    char macstr[18];
    unpackmac(e1000->macaddr, macstr);
    macstr[17] = 0;
    
    cprintf("\nMAC Address of the E1000 device:%s\n", macstr);
    struct e1000tbd ** ttmp = (struct e1000tbd *)kalloc();
    for (i = 0; i < E1000_TBD_SLOTS; i++, ttmp++)
	e1000->tbd[i] = (struct e1000tbd *)ttmp;
    
    if ((V2P(e1000->tbd[0]) & 0x0000000f) != 0) {
	cprintf("Error: e1000 : Transmit Descriptor Ring not on paragraph boundary\n");
	kfree((char *)ttmp);
	return -1;
    }
    
    struct e1000rbd * rtmp = (struct e1000rbd *)kalloc();
    for (i = 0; i < E1000_RBD_SLOTS; i++, rtmp++)
	e100->rbd[i] = (struct e100rbd *)rtmp;
    
    if ((V2P(e1000->rbd[0]) & 0x0000000f) != 0) {
	cprintf("ERROR: E1000 : Receive Descriptor Ring not on paragraph boundary\n");
	kfree((char *)rtmp);
	return -1;
    }
    
    struct packetbuf * tmp;
    for (i = 0; i < E1000_RBD_SLOTS; i += 2) {
	tmp = (struct packetbuf *)kalloc();
	e1000->rbuf[i] = tmp++;
	e1000->rbd[i]->addrl = V2P((uint32_t)e1000->rbuf[i]);
	e1000->rbd[i]->addrh = 0;
	e1000->rbuf[i + 1] = tmp;
	e1000->rbd[i + 1]->addrl = V2P((uint32_t)e1000->rbuf[i + 1]);
	e1000->rbd[i + 1]->addrh = 0;
    }
    
    for (i = 0; i < E1000_TBD_SLOTS; i += 2) {
	tmp = (struct packetbuf *)kalloc();
	e1000->tbuf[i] = tmp++;
	// e1000->tbd[i]->addr = (uint32_t)e1000->tbuf[i];
	// e1000->tbd[i]->addrh = 0;
	e1000->tbuf[i + 1] = tmp;
	// e1000->tbd[i + 1]->addrl = (uint32_t)e1000->tbuf[i + 1];
	// e1000->tbd[i + 1]->addrh = 0;
    }
    
    // Populate the Descriptor Ring Addresses in TDBAL and RDBAL, plus HEAD and TAIL pointers
    e1000regwrite(E1000_TDBAL, V2P(e1000->tbd[0]), e1000);
    e1000regwrite(E1000_TDBAH, 0x00000000, e1000);
    e1000regwrite(E1000_TDLEN, (E1000_TDB_SLOTS * 16) << 7, e1000);
    e1000regwrite(E1000_TDH, 0x00000000, e1000);
    e1000regwrite(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_SET(0x0f) | E1000_TCTL_COLD_SET(0x200), e1000);
    e100regwrite(E1000_TDT, 0, e1000);
    e1000regwrite(E1000_TIPG, E1000_TIPG_IPGT_SET(10) | E1000_TIPG_IPGR1_SET(10) | E1000_TIPG_IPGR2_SET(10), e1000);
    e1000regwrite(E1000_RDBAL, V2P(e1000->rbd[0]), e1000);
    e1000regwrite(E1000_RDBAH, 0x00000000, e1000);
    e1000regwrite(E1000_RDLEN, (E1000_RDB_SLOTS * 16) << 7, e1000);
    e1000regwrite(E1000_RDH, 0x00000000, e1000);
    e1000regwrite(E1000_RDT, 0x00000000, e1000);
    e1000regwrite(E1000_IMS, E1000_IMS_RXSEQ | E1000_IMS_RXO | E1000_IMS_RXT0 | E1000_IMS_TXQE, e1000);
    e1000regwrite(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE | 0x00000008, e1000); // E1000_RCTL_SECRC
    
    cprintf("E1000: Interrupt enabled mask:0x%x\n", e1000regread(E1000_IMS, e1000));
    // TODO: Interrupt Handler
    picenable(e1000->irqline);
    ioapicenable(e1000->irqline, 0);
    ioapicenable(e1000->irqline, 1);
    
    * drv = e1000;
    return 0;
} 

void recve1000(void * drv, uint8_t * pkt, uint16_t len) {
    // TODO: Implement Receieve
}