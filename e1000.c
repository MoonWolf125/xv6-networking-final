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

#define E1000_CONTROL_RST_BIT(cntrl) \
        (cntrl & E1000_CONTROL_RESET_MASK)

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
 * Ethernet Device Interrupt Mast Set
 * 	Transmit Queue Empty
 * 	Receive Sequence Error
 * 	Receiver Overrun
 * 	Receiver Timer Interrupt
 */
#define E1000_IMS		0x000d0
#define E1000_IMS_TXQE		0x00000002
#define E1000_IMS_RXSEQ		0x00000008
#define E1000_IMS_RXO		0x00000040
#define E1000_IMS_RXT0		0x00000080

/*
 * Ethernet Device Receiver Controller
 * 	Enable*
 * 	Broadcast Accept Mode
 * 	Receive Buffer Size
 * 	Strip Ethernet CRC
 */
#define E1000_RCTL		0x00100
#define E1000_RCTL_EN		0x00000002
#define E1000_RCTL_BAM		0x00008000
#define E1000_RCTL_BSIZE	0x00000000
#define E1000_RCTL_SECRC	0x04000000

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
 * 	Queue must be aligned on 16B boundary
 */
typedef struct {
    uint64_t addr;	// Buffer Address
    uint16_t len;	// Length
    uint8_t cso;	// Checksum Offset
    uint8_t cmd;	// Command
    uint8_t sts;	// Status
    uint8_t css;	// Checksum Start
    uint16_t spc;	// Special
} e1000_TBD;

/*
 * Receive Buffer Descriptor
 * 	Queue must be aligned on 16B boundary
 */
typedef struct {
    uint32_t addr0;	// First Buffer Address
    uint32_t addr1;	// Second Buffer Address
    uint16_t len;	// Length
    uint16_t csm;	// Checksum
    uint8_t sts;	// Status
    uint8_t err;	// Errors
    uint16_t spc;	// Special
} e1000_RBD;

typedef struct {
    uint8_t buf[2046];
} packbuf;

/*
 * E1000 Structure
 */
typedef struct {
    // Transmit and Receive Buffer Descriptors
    e1000_TBD * tbd[E1000_TBD_SLOTS];
    e1000_RBD * rbd[E1000_RBD_SLOTS];
    
    // Packet buffer space for Transmit and Receieve
    packbuf * tbuf[E1000_TBD_SLOTS];
    packbuf * rbuf[E1000_RBD_SLOTS];
    
    int tbdhead, tbdtail, rbdhead, rbdtail;
    char tbdidle, rbdidle;
    uint32_t iobase;
    uint32_t membase;
    uint8_t irqline;	// Interrupt Request Line
    uint8_t irqpin;	// Interrupt Request Pin
    uint8_t mac[6];
} e1000;

static void e1000regwrite(uint32_t regaddr, uint32_t val, e1000 * e1000) {
    * (uint32_t *)(e1000->membase + regaddr) = val;
}

static uint32_t e1000regread(uint32_t regaddr, e1000 * e1000) {
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
    e1000 * _e1000 = (e1000 *)kalloc();
    _e1000 = drv;
    cprintf("E1000 driver: Sending packet of length: 0x%x %x starting at physical address: 0x%x\n", len, sizeof(eth_head), V2P(_e1000->tbuf[_e1000->tbdtail]));
    memset(_e1000->tbd[_e1000->tbdtail], 0, sizeof(e1000_TBD));
    memmove((_e1000->tbuf[_e1000->tbdtail]), pkt, len);
    _e1000->tbd[_e1000->tbdtail]->addr = (uint64_t)(uint32_t)V2P(_e1000->tbuf[_e1000->tbdtail]);
    _e1000->tbd[_e1000->tbdtail]->len = len;
    _e1000->tbd[_e1000->tbdtail]->cmd = (E1000_TDESC_CMD_RS | E1000_TDESC_CMD_EOP | E1000_TDESC_CMD_IFCS);
    _e1000->tbd[_e1000->tbdtail]->cso = 0;
    int oldtail = _e1000->tbdtail;
    _e1000->tbdtail = (_e1000->tbdtail + 1) % E1000_TBD_SLOTS;
    e1000regwrite(E1000_TDT, _e1000->tbdtail, _e1000);
    
    while (!E1000_TDESC_STATUS_DONE(_e1000->tbd[oldtail]->sts)) {
	delay(2);
    }
    cprintf("after while loop\n");
}

int inite1000(pcifunc * pcif, void ** drv, uint8_t * macaddr) {
    e1000 * _e1000 = (e1000 *)kalloc();
    int i;
    for (i = 0; i < 6; i = i + 1) {
	if (pcif->regbase[i] <= 0xffff) {
	    _e1000->iobase = pcif->regbase[i];
	    if (pcif->regsize[i] != 64) {
		panic("I/O space BAR size != 64");
	    }
	    break;
	}
	else if (pcif->regbase[i] > 0) {
	    _e1000->membase = pcif->regbase[i];
	    if (pcif->regsize[i] != (1<<17))
		panic("Mem space BAR size != 128KB");
	}
    }
    if (!_e1000->iobase) {
	panic("Failed to find a valid I/O port base for the E1000");
	if (!_e1000->membase)
	    panic("Fail to find a valid Mem I/O base for E1000");
    }
    _e1000->irqline = pcif->irqline;
    _e1000->irqpin = pcif->irqpin;
    cprintf("E1000 init: Interrupt pin=%d and line:%d\n", _e1000->irqpin, _e1000->irqline);
    _e1000->tbdhead = _e1000->tbdtail = 0;
    _e1000->rbdhead = _e1000->rbdtail = 0;
    
    // Reset the device
    e1000regwrite(E1000_CONTROL_REG,
		  e1000regread(E1000_CONTROL_REG, _e1000) | E1000_CONTROL_RESET_MASK, _e1000);
    do {
	delay(3);
    } while (E1000_CONTROL_RST_BIT(e1000regread(E1000_CONTROL_REG, _e1000)));
    
    uint32_t cntrlreg = e1000regread(E1000_CONTROL_REG, _e1000);
    e1000regwrite(E1000_CONTROL_REG, cntrlreg | E1000_CONTROL_ASDE_MASK | E1000_CONTROL_SLUP_MASK, _e1000);
    
    uint32_t macaddrl = e1000regread(E1000_RECV_RAL0, _e1000);
    uint32_t macaddrh = e1000regread(E1000_RECV_RAH0, _e1000);
    * (uint32_t *)_e1000->mac = macaddrl;
    * (uint16_t *)(&_e1000->mac[4]) = (uint16_t)macaddrh;
    * (uint32_t *)macaddr = macaddrl;
    * (uint32_t *)(&macaddr[4]) = (uint16_t)macaddrh;
    char macstr[18];
    unpackmac(_e1000->mac, macstr);
    macstr[17] = 0;
    
    cprintf("\nMAC Address of the E1000 device:%s\n", macstr);
    e1000_TBD * ttmp = (e1000_TBD *)kalloc();
    for (i = 0; i < E1000_TBD_SLOTS; i++, ttmp++)
	_e1000->tbd[i] = (e1000_TBD *)ttmp;
    
    if ((V2P(_e1000->tbd[0]) & 0x0000000f) != 0) {
	cprintf("Error: _e1000 : Transmit Descriptor Ring not on paragraph boundary\n");
	kfree((char *)ttmp);
	return -1;
    }
    
    e1000_RBD * rtmp = (e1000_RBD *)kalloc();
    for (i = 0; i < E1000_RBD_SLOTS; i++, rtmp++)
	_e1000->rbd[i] = (e1000_RBD *)rtmp;
    
    if ((V2P(_e1000->rbd[0]) & 0x0000000f) != 0) {
	cprintf("ERROR: E1000 : Receive Descriptor Ring not on paragraph boundary\n");
	kfree((char *)rtmp);
	return -1;
    }
    
    packbuf * tmp;
    for (i = 0; i < E1000_RBD_SLOTS; i += 2) {
	tmp = (packbuf *)kalloc();
	_e1000->rbuf[i] = tmp++;
	_e1000->rbd[i]->addr0 = V2P((uint32_t)_e1000->rbuf[i]);
	_e1000->rbd[i]->addr1 = 0;
	_e1000->rbuf[i + 1] = tmp;
	_e1000->rbd[i + 1]->addr0 = V2P((uint32_t)_e1000->rbuf[i + 1]);
	_e1000->rbd[i + 1]->addr1 = 0;
    }
    
    for (i = 0; i < E1000_TBD_SLOTS; i += 2) {
	tmp = (packbuf *)kalloc();
	_e1000->tbuf[i] = tmp++;
	// _e1000->tbd[i]->addr = (uint32_t)_e1000->tbuf[i];
	// _e1000->tbd[i]->addrh = 0;
	_e1000->tbuf[i + 1] = tmp;
	// _e1000->tbd[i + 1]->addrl = (uint32_t)_e1000->tbuf[i + 1];
	// _e1000->tbd[i + 1]->addrh = 0;
    }
    
    // Populate the Descriptor Ring Addresses in TDBAL and RDBAL, plus HEAD and TAIL pointers
    e1000regwrite(E1000_TDBAL, V2P(_e1000->tbd[0]), _e1000);
    e1000regwrite(E1000_TDBAH, 0x00000000, _e1000);
    e1000regwrite(E1000_TDLEN, (E1000_TBD_SLOTS * 16) << 7, _e1000);
    e1000regwrite(E1000_TDH, 0x00000000, _e1000);
    e1000regwrite(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_SET(0x0f) | E1000_TCTL_COLD_SET(0x200), _e1000);
    e1000regwrite(E1000_TDT, 0, _e1000);
    e1000regwrite(E1000_TIPG, E1000_TIPG_IPGT_SET(10) | E1000_TIPG_IPGR1_SET(10) | E1000_TIPG_IPGR2_SET(10), _e1000);
    e1000regwrite(E1000_RDBAL, V2P(_e1000->rbd[0]), _e1000);
    e1000regwrite(E1000_RDBAH, 0x00000000, _e1000);
    e1000regwrite(E1000_RDLEN, (E1000_RBD_SLOTS * 16) << 7, _e1000);
    e1000regwrite(E1000_RDH, 0x00000000, _e1000);
    e1000regwrite(E1000_RDT, 0x00000000, _e1000);
    e1000regwrite(E1000_IMS, E1000_IMS_RXSEQ | E1000_IMS_RXO | E1000_IMS_RXT0 | E1000_IMS_TXQE, _e1000);
    e1000regwrite(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE | 0x00000008, _e1000); // E1000_RCTL_SECRC
    
    cprintf("E1000: Interrupt enabled mask:0x%x\n", e1000regread(E1000_IMS, _e1000));
    // TODO: Interrupt Handler
    picenable(_e1000->irqline);
    ioapicenable(_e1000->irqline, 0);
    ioapicenable(_e1000->irqline, 1);
    
    * drv = _e1000;
    return 0;
} 

void recve1000(void * drv, uint8_t * pkt, uint16_t len) {
    // TODO: Implement Receieve
}