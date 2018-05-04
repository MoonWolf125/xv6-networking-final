#include "types.h"
#include "x86.h"
#include "traps.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)
#define IRQ_SLAVE	2

static ushort irqmask = 0xFFFF & ~(1<<IRQ_SLAVE);

static void picsetmask(ushort mask) {
    irqmask = mask;
    outb(IO_PIC1 + 1, mask);
    outb(IO_PIC2 + 1, mask >> 8);
}

void picenable(int irq) {
    picsetmask(irqmask & ~(1 << irq));
}

// Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
void
picinit(void)
{
  // mask all interrupts
  outb(IO_PIC1+1, 0xFF);
  outb(IO_PIC2+1, 0xFF);

    outb(IO_PIC1, 0x11);
    outb(IO_PIC1 + 1, T_IRQ0);
    outb(IO_PIC1 + 1, 1 << IRQ_SLAVE);
    outb(IO_PIC1 + 1, 0x3);

    outb(IO_PIC2, 0x11);
    outb(IO_PIC2 + 1, T_IRQ0 + 8);
    outb(IO_PIC2 + 1, IRQ_SLAVE);
    outb(IO_PIC2 + 1, 0x3);

    outb(IO_PIC1, 0x68);
    outb(IO_PIC1, 0x0a);
    outb(IO_PIC2, 0x68);
    outb(IO_PIC2, 0x0a);

    if(irqmask != 0xFFFF)
	picsetmask(irqmask);
}
//PAGEBREAK!
// Blank page.
