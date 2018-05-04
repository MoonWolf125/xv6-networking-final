#include "pci.h"
#include "x86.h"
#include "defs.h"
#include "pciregs.h"
#include "e1000.h"
#include "nic.h"

static const char * pcicls[] = {"Unclassified Device",
    "Mass Storage Controller", "Network Controller",
    "Display Controller", "Multimedia Device",
    "Memory Controller", "Bridge Device"};

static void pciprntfunc(pcifunc * f) {
    const char * class = pcicls[0];
    if (PCI_CLASS(f->devcls) < sizeof(pcicls) / sizeof(pcicls[0]))
	class = pcicls[PCI_CLASS(f->devcls)];
    
    cprintf("PCI: %x:%x.%d: %x:%x: class: %x.%x (%s) irq: %d\n",
	    f->bus->busnum, f->dev, f->func,
	    PCI_VENDOR(f->devcls), PCI_SUBCLASS(f->devcls), class, f->irqline);
}

static uint32_t pciconfigformaddr(uint32_t busaddr, uint32_t devaddr,
				  uint32_t funcaddr, uint32_t offset) {
    uint32_t val = 0x80000000 | busaddr << 16 | devaddr << 11 | funcaddr << 8 | offset;
    return val;
}

static uint32_t pciconfread(pcifunc * f, uint32_t offset) {
    uint32_t val = pciconfigformaddr(f->bus->busnum, f->dev, f->func, offset);
    outl(PCI_CONF_DATA_IOPORT, val);
    return inl(PCI_CONF_DATA_IOPORT);
}

static void pciconfwrite(pcifunc * f, uint32_t offset, uint32_t val) {
    uint32_t address = pciconfigformaddr(f->bus->busnum, f->dev, f->func, offset);
    outl(PCI_CONF_ADDR_IOPORT, address);
    outl(PCI_CONF_DATA_IOPORT, val);
}

void pcienabledev(pcifunc * f) {
    pciconfwrite(f, PCI_COMMAND_STATUS_REG, PCI_COMMAND_IO_ENABLE |
		 PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);
    cprintf("pcicmd reg:0x%x\n", pciconfread(f, PCI_COMMAND_STATUS_REG));
    
    uint32_t barwidth;
    uint32_t bar;
    for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END; bar += barwidth) {
	uint32_t oldv = pciconfread(f, bar);
	barwidth = 4;
	pciconfwrite(f, bar, 0xffffffff);
	uint32_t rv = pciconfread(f, bar);
	if (rv == 0)
	    continue;
	
	int regnum = PCI_MAPREG_NUM(bar);
	uint32_t base, size;
	if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
	    if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
		barwidth = 8;
	    
	    size = PCI_MAPREG_MEM_SIZE(rv);
	    base = PCI_MAPREG_MEM_ADDR(oldv);
	    cprintf("mem region %d: %d bytes at 0x%x\n", regnum, size, base);
	}
	else {
	    size = PCI_MAPREG_IO_SIZE(rv);
	    base = PCI_MAPREG_IO_ADDR(oldv);
	    cprintf("io region %d: %d bytes at 0x%x\n", regnum, size, base);
	}
	
	pciconfwrite(f, bar, oldv);
	f->regbase[regnum] = base;
	f->regsize[regnum] = size;
	
	if (size && !base)
	    cprintf("PCI device %x:%x.%d (%x:%x) may be misconfigured: "
		    "region %d: base 0x%x, size %d\n", f->bus->busnum, f->dev, f->func,
		    PCI_VENDOR(f->devid), PCI_PRODUCT(f->devid), regnum, base, size);
    }
}

static int attache1000(pcifunc * pcifunc) {
    pcienabledev(pcifunc);
    nic d;
    inite1000(pcifunc, &d.drvr, d.macaddr);
    d.sendpacket = sende1000;
    d.recvpacket = recve1000;
    regnicdevice(d);
    return 0;
}

pcidrvr attachpcivendbase[] = {{0x8086, 0x100e, attache1000}, {0, 0, 0},};

static void attachpcinic(pcifunc * pcifunc) {
    uint i;
    uint32_t vendid = PCI_VENDOR(pcifunc->devid);
    uint32_t prodid = PCI_PRODUCT(pcifunc->devid);
    pcidrvr * list = &attachpcivendbase[0];
    
    for (i = 0; list[i].atchfunc; i++) {
	if (list[i].key0 == vendid && list[i].key1 == prodid) {
	    int r = list[i].atchfunc(pcifunc);
	    if (r < 0)
		cprintf("PCI Attach Match: Attaching %x.%x (%p): %e\n",
			vendid, prodid, list[i].atchfunc, r);
	}
    }
}

static int pcienumbus(pcibus * bus) {
    int totdev = 0;
    pcifunc func;
    memset(&func, 0, sizeof(func));
    func.bus = bus;
    
    // Enum over the PCI bus. For each devnum check if supported device connected
    for (func.dev = 0; func.dev < MAX_DEV_PER_PCI_BUS; func.dev++) {
	uint32_t bhlc = pciconfread(&func, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) > 1)
	    continue;
	totdev++;
	
	pcifunc f = func;
	for (f.func = 0; f.func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1); f.func++) {
	    pcifunc pf = f;
	    pf.devid = pciconfread(&f, PCI_ID_REG);
	    if (PCI_VENDOR(pf.devid) == 0xffff)
		continue;
	    uint32_t intr = pciconfread(&pf, PCI_INTERRUPT_REG);
	    pf.irqline = PCI_INTERRUPT_LINE(intr);
	    pf.irqpin = PCI_INTERRUPT_PIN(intr);
	    pf.devcls = pciconfread(&pf, PCI_CLASS_REG);
	    pciprntfunc(&pf);
	    
	    if (PCI_CLASS(pf.devcls) == PCI_DEVICE_CLASS_NETWORK_CONTROLLER)
		attachpcinic(&pf);
	}
    }
    return totdev;
}

int pciinit(void) {
    static pcibus rootbus;
    memset(&rootbus, 0, sizeof(rootbus));
    return pcienumbus(&rootbus);
}