#ifndef __XV6_NETSTACK_PCI_H__
#define __XV6_NETSTACK_PCI_H__

#include "types.h"

// // Since PCI bus addresses have 8-bit for PCI bus,
// // 5-bit for device , and 3-bit for function numbers for the device
// // So a total of 2^5 devices per bus
// #define MAX_DEVICE_PER_PCI_BUS 32


enum { pci_res_bus, pci_res_mem, pci_res_io, pci_res_max };

struct pci_bus;

struct pci_func {
    struct pci_bus *bus;    // Primary bus for bridges

    uint32_t dev;
    uint32_t func;

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};

struct pci_bus {
    struct pci_func *parent_bridge;
    uint32_t busno;
};

int  pci_init(void);
void pci_func_enable(struct pci_func *f);

#endif
