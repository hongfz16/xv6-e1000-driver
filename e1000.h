#ifndef __XV6_NETSTACK_e1000_H__
#define __XV6_NETSTACK_e1000_H__
#include "types.h"
#include "nic.h"
#include "pci.h"

#define E1000_CNTRL_RST_BIT(cntrl) \
        (cntrl & E1000_CNTRL_RST_MASK)
#define E1000_TCTL_CT_SET(value) \
        ((value << E1000_TCTL_CT_BIT_SHIFT) & E1000_TCTL_CT_BIT_MASK)
#define E1000_TCTL_COLD_BIT_MASK  0x003ff000
#define E1000_TCTL_COLD_BIT_SHIFT 12
#define E1000_TCTL_COLD_SET(value) \
        ((value << E1000_TCTL_COLD_BIT_SHIFT) & E1000_TCTL_COLD_BIT_MASK)
#define E1000_TIPG_IPGT_SET(value) \
        ((value << E1000_TIPG_IPGT_BIT_SHIFT) & E1000_TIPG_IPGT_BIT_MASK)
#define E1000_TIPG_IPGR1_BIT_MASK   0x000ffc00
#define E1000_TIPG_IPGR1_BIT_SHIFT   10
#define E1000_TIPG_IPGR1_SET(value) \
        ((value << E1000_TIPG_IPGR1_BIT_SHIFT) & E1000_TIPG_IPGR1_BIT_MASK)
#define E1000_TIPG_IPGR2_BIT_MASK   0x3ff00000
#define E1000_TIPG_IPGR2_BIT_SHIFT   20
#define E1000_TIPG_IPGR2_SET(value) \
        ((value << E1000_TIPG_IPGR2_BIT_SHIFT) & E1000_TIPG_IPGR2_BIT_MASK)
#define E1000_TDESC_STATUS_DONE(status) \
        (status & E1000_TDESC_STATUS_DONE_MASK)

#define E1000_EERD_ADDR(addr) \
        ((addr << E1000_EERD_ADDR_BIT_SHIFT) & E1000_EERD_ADDR_BIT_MASK)

#define E1000_EERD_DATA(eerd) \
        (eerd >> E1000_EERD_DATA_BIT_SHIFT)

#define E1000_EERD_DONE(eerd) \
        (eerd & E1000_EERD_DONE_BIT_MASK)

#define E1000_VENDOR 0x8086
#define E1000_DEVICE 0x100E

#define NTXDESC 64
#define NRXDESC 128

#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */
#define E1000_RAL       0x05400  /* Receive Address - RW Array */
#define E1000_RAH       0x05404  /* Receive Address - RW Array */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */


#define SET_E1000_REG(i) (e1000+i/4)
#define GET_E1000_REG(i) (e1000+i/4)

struct tx_desc{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

struct rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

volatile uint32_t *e1000;
struct tx_desc tx_desc_table[NTXDESC];
struct rx_desc rx_desc_table[NRXDESC];
volatile uint32_t *tdt;
volatile uint32_t *rdt;

int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length);

#endif
