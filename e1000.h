#ifndef __XV6_NETSTACK_e1000_H__
#define __XV6_NETSTACK_e1000_H__
/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *device driver for the E1000 emulated NIC on an x86 core
 *https://pdos.csail.mit.edu/6.828/2017/readings/hardware/8254x_GBe_SDM.pdf
 */
#include "types.h"
#include "nic.h"
#include "pci.h"

#define E1000_VENDOR 0x8086
#define E1000_DEVICE 0x100E

#define E1000_RBD_SLOTS			128
#define E1000_TBD_SLOTS			128

//Bit 31:20 are not writable. Always read 0b.
#define E1000_IOADDR_OFFSET 0x00000000

#define E1000_IODATA_OFFSET 0x00000004

/**
 * Ethernet Device Control Register values
 */
#define E1000_CNTRL_REG           0x00000

//Global device reset. does not clear PCI config
#define E1000_CNTRL_RST_MASK      0x04000000
#define E1000_CNTRL_ASDE_MASK     0x00000020
#define E1000_CNTRL_SLU_MASK      0x00000040


#define E1000_CNTRL_RST_BIT(cntrl) \
        (cntrl & E1000_CNTRL_RST_MASK)

/**
 * Ethernet Device registers
 */
#define E1000_RCV_RAL0      0x05400
#define E1000_RCV_RAH0      0x05404
#define E1000_TDBAL         0x03800
#define E1000_TDBAH         0x03804
#define E1000_TDLEN         0x03808
#define E1000_TDH           0x03810
#define E1000_TDT           0x03818
#define E1000_RDBAL         0x02800
#define E1000_RDBAH         0x02804
#define E1000_RDLEN         0x02808
#define E1000_RDH           0x02810
#define E1000_RDT           0x02818

/**
 * Ethernet Device Transmission Control register
 */
#define E1000_TCTL                0x00400

#define E1000_TCTL_EN             0x00000002
#define E1000_TCTL_PSP            0x00000008
#define E1000_TCTL_CT_BIT_MASK    0x00000ff0
#define E1000_TCTL_CT_BIT_SHIFT   4
#define E1000_TCTL_CT_SET(value) \
        ((value << E1000_TCTL_CT_BIT_SHIFT) & E1000_TCTL_CT_BIT_MASK)
#define E1000_TCTL_COLD_BIT_MASK  0x003ff000
#define E1000_TCTL_COLD_BIT_SHIFT 12
#define E1000_TCTL_COLD_SET(value) \
        ((value << E1000_TCTL_COLD_BIT_SHIFT) & E1000_TCTL_COLD_BIT_MASK)

/**
 * Ethernet Device Transmission Inter-Packet Gap register
 */
#define E1000_TIPG                0x00410

#define E1000_TIPG_IPGT_BIT_MASK    0x000003ff
#define E1000_TIPG_IPGT_BIT_SHIFT   0
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

/**
* Ethernet Device Interrupt Mast Set registers
*/
#define E1000_IMS                 0x000d0
#define E1000_IMS_TXQE            0x00000002
#define E1000_IMS_LSC             0x00000004
#define E1000_IMS_RXSEQ           0x00000008
#define E1000_IMS_RXDMT0          0x00000010
#define E1000_IMS_RXO             0x00000040
#define E1000_IMS_RXT0            0x00000080

#define E1000_MTA                 0X05200

/**
 * Ethernet Device Receive Control register
 */
#define E1000_RCTL                0x00100

#define E1000_RCTL_EN             0x00000002
#define E1000_RCTL_BAM            0x00008000
#define E1000_RCTL_BSIZE          0x00000000
#define E1000_RCTL_SECRC          0x04000000
#define E1000_RCTL_UPE            0x00000008

#define E1000_RCTL_LBM_MAC        0x00000040
#define E1000_RCTL_LBM_SLP        0x00000080
#define E1000_RCTL_LBM_TCVR       0x000000C0
#define E1000_RCTL_VFE            0x00040000

/**
 * Ethernet Device Transmit Descriptor Command Field
 */
#define E1000_TDESC_CMD_RS      0x08
#define E1000_TDESC_CMD_EOP     0x01
#define E1000_TDESC_CMD_IFCS    0x02

/**
 * Ethernet Device Transmit Descriptor Status Field
 */
#define E1000_TDESC_STATUS_DONE_MASK   0x01
#define E1000_TDESC_STATUS_DONE(status) \
        (status & E1000_TDESC_STATUS_DONE_MASK)

/**
  * Ethernet Device EEPROM registers
  */
#define E1000_EERD_REG_ADDR         0x00014

#define E1000_EERD_START_BIT_MASK   0x00000001
#define E1000_EERD_ADDR_BIT_MASK    0x0000ff00
#define E1000_EERD_ADDR_BIT_SHIFT   8
#define E1000_EERD_DATA_BIT_MASK    0xffff0000
#define E1000_EERD_DATA_BIT_SHIFT   16
#define E1000_EERD_DONE_BIT_MASK    0x00000010

#define E1000_EERD_ADDR(addr) \
        ((addr << E1000_EERD_ADDR_BIT_SHIFT) & E1000_EERD_ADDR_BIT_MASK)

#define E1000_EERD_DATA(eerd) \
        (eerd >> E1000_EERD_DATA_BIT_SHIFT)

#define E1000_EERD_DONE(eerd) \
        (eerd & E1000_EERD_DONE_BIT_MASK)

/**
 * EEPROM Address Map
 */
#define EEPROM_ADDR_MAP_ETHER_ADDR_1    0x00
#define EEPROM_ADDR_MAP_ETHER_ADDR_2    0x01
#define EEPROM_ADDR_MAP_ETHER_ADDR_3    0x02

#define E1000_MANC              0x05820
#define E1000_MANC_ARP_EN       0x00002000
#define E1000_MANC_ARP_RES_EN   0x00008000


// #define E1000_VENDOR 0x8086
// #define E1000_DEVICE 0x100E

// #define NTXDESC 64
// #define NRXDESC 128

// #define E1000_STATUS   0x00008  /* Device Status - RO */
// #define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
// #define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
// #define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
// #define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
// #define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
// #define E1000_TCTL     0x00400  /* TX Control - RW */
//     #define E1000_TCTL_EN     0x00000002    /* enable tx */
//     #define E1000_TCTL_PSP    0x00000008    /* pad short packets */
//     #define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
//     #define E1000_TCTL_COLD   0x003ff000    /* collision distance */
// #define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
// #define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
// #define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */
// #define E1000_RAL       0x05400  /* Receive Address - RW Array */
// #define E1000_RAH       0x05404  /* Receive Address - RW Array */
// #define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
// #define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
// #define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
// #define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
// #define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
// #define E1000_RCTL     0x00100  /* RX Control - RW */
// #define E1000_RCTL_EN             0x00000002    /* enable */
// #define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
// #define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
// #define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
// #define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */


// #define E1000_REG(i) (e1000+i/4)

// struct tx_desc{
//     uint64_t addr;
//     uint16_t length;
//     uint8_t cso;
//     uint8_t cmd;
//     uint8_t status;
//     uint8_t css;
//     uint16_t special;
// };

// struct rx_desc {
//     uint64_t addr;
//     uint16_t length;
//     uint16_t checksum;
//     uint8_t	status;
//     uint8_t	errors;
//     uint16_t special;
// };

// volatile uint32_t *e1000;
// struct tx_desc tx_desc_table[NTXDESC];
// struct rx_desc rx_desc_table[NRXDESC];
// volatile uint32_t *tdt;
// volatile uint32_t *rdt;


//Trasmit Buffer Descriptor
// The Transmit Descriptor Queue must be aligned on 16-byte boundary
__attribute__ ((packed))
struct e1000_tbd {
  uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

//Receive Buffer Descriptor
// The Receive Descriptor Queue must be aligned on 16-byte boundary
__attribute__ ((packed))
struct e1000_rbd {
  // uint32_t addr_l;
  // uint32_t addr_h;
	uint64_t addr;
  uint16_t	length;
	uint16_t	checksum;
	uint8_t	status;
	uint8_t	errors;
	uint16_t	special;
};

struct packet_buf {
    uint8_t buf[2046];
};

struct e1000 {
	struct e1000_tbd *tbd[E1000_TBD_SLOTS];
	struct e1000_rbd *rbd[E1000_RBD_SLOTS];

  struct packet_buf *tx_buf[E1000_TBD_SLOTS];  //packet buffer space for tbd
  struct packet_buf *rx_buf[E1000_RBD_SLOTS];  //packet buffer space for rbd

  int tbd_head;
	int tbd_tail;
	char tbd_idle;

	int rbd_head;
	int rbd_tail;
	char rbd_idle;

  uint32_t iobase;
  uint32_t membase;
  uint8_t irq_line;
  uint8_t irq_pin;
  uint8_t mac_addr[6];
};

int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length);

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

// uint8_t e100_irq;

// #define E100_TX_SLOTS     64
// #define E100_RX_SLOTS     64

// #define E100_NULL     0xffffffff
// #define E100_SIZE_MASK      0x3fff  // mask out status/control bits

// #define E100_CSR_SCB_STATACK    0x01  // scb_statack (1 byte)
// #define E100_CSR_SCB_COMMAND    0x02  // scb_command (1 byte)
// #define E100_CSR_SCB_GENERAL    0x04  // scb_general (4 bytes)
// #define E100_CSR_PORT     0x08  // port (4 bytes)

// #define E100_PORT_SOFTWARE_RESET  0

// #define E100_SCB_COMMAND_CU_START 0x10
// #define E100_SCB_COMMAND_CU_RESUME  0x20

// #define E100_SCB_COMMAND_RU_START 1
// #define E100_SCB_COMMAND_RU_RESUME  2

// #define E100_SCB_STATACK_RNR    0x10
// #define E100_SCB_STATACK_CNA    0x20
// #define E100_SCB_STATACK_FR   0x40
// #define E100_SCB_STATACK_CXTNO    0x80

// // commands
// #define E100_CB_COMMAND_XMIT    0x4

// // command flags
// #define E100_CB_COMMAND_SF    0x0008  // simple/flexible mode
// #define E100_CB_COMMAND_I   0x2000  // interrupt on completion
// #define E100_CB_COMMAND_S   0x4000  // suspend on completion

// #define E100_CB_STATUS_C    0x8000

// #define E100_RFA_STATUS_OK    0x2000  // packet received okay
// #define E100_RFA_STATUS_C   0x8000  // packet reception complete

// #define E100_RFA_CONTROL_SF   0x0008  // simple/flexible memory mode
// #define E100_RFA_CONTROL_S    0x4000  // suspend after reception

// struct e100_cb_tx {
//   volatile uint16_t cb_status;
//   volatile uint16_t cb_command;
//   volatile uint32_t link_addr;
//   volatile uint32_t tbd_array_addr;
//   volatile uint16_t byte_count;
//   volatile uint8_t tx_threshold;
//   volatile uint8_t tbd_number;
// };

// // Transmit Buffer Descriptor (TBD)
// struct e100_tbd {
//   volatile uint32_t tb_addr;
//   volatile uint16_t tb_size;
//   volatile uint16_t tb_pad;
// };

// // Receive Frame Area (RFA)
// struct e100_rfa {
//   // Fields common to all i8255x chips.
//   volatile uint16_t rfa_status;
//   volatile uint16_t rfa_control;
//   volatile uint32_t link_addr;
//   volatile uint32_t rbd_addr;
//   volatile uint16_t actual_size;
//   volatile uint16_t size;
// };

// // Receive Buffer Descriptor (RBD)
// struct e100_rbd {
//   volatile uint16_t rbd_count;
//   volatile uint16_t rbd_pad0;
//   volatile uint32_t rbd_link;
//   volatile uint32_t rbd_buffer;
//   volatile uint16_t rbd_size;
//   volatile uint16_t rbd_pad1;
// };

// struct e100_tx_slot {
//   struct e100_cb_tx tcb;
//   struct e100_tbd tbd;
//   // Some cards require two TBD after the TCB ("Extended TCB")
//   struct e100_tbd unused;
//   uint8_t* p;
// };

// struct e100_rx_slot {
//   struct e100_rfa rfd;
//   struct e100_rbd rbd;
//   uint8_t *p;
//   unsigned int offset;
// };

// struct e100 {
//   uint32_t iobase;

//   struct e100_tx_slot tx[E100_TX_SLOTS];
//   int tx_head;
//   int tx_tail;
//   char tx_idle;

//   struct e100_rx_slot rx[E100_RX_SLOTS];
//   int rx_head;
//   int rx_tail;
//   char rx_idle;
// };


// int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

// void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
// void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length);
// void e100_intr(void);
// void udelay(unsigned int u);

//>>>>>>>>>>>>>>>>>>>>>>

// #define MAX_ETH_FRAME 1518

// #define TCB_COUNT 16
// #define RFD_COUNT 16

#define ROUNDDOWN(a, n)           \
({                \
  uint32_t __a = (uint32_t) (a);        \
  (typeof(a)) (__a - __a % (n));        \
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)           \
({                \
  uint32_t __n = (uint32_t) (n);        \
  (typeof(a)) (ROUNDDOWN((uint32_t) (a) + __n - 1, __n)); \
})

// struct cb {
//   volatile uint16_t status;
//   uint16_t cmd;
//   uint32_t link;
// };

// struct tcb {
//   struct cb cb;
//   uint32_t tbd_array_addr;
//   uint16_t tbd_byte_count;
//   uint16_t tbd_thrs;
//   uint8_t pkt_data[MAX_ETH_FRAME];
// };

// struct rfd {
//   struct cb cb;
//   uint32_t reserved;
//   uint16_t actual_count;
//   uint16_t buffer_size;
//   uint8_t pkt_data[MAX_ETH_FRAME];
// };

// struct pci_record {
//   uint32_t reg_base[6];
//   uint32_t reg_size[6];
//   uint8_t irq_line;
// };

// struct pci_record pcircd;
// struct e100
// {
//   struct tcb tcbring[TCB_COUNT];
//   struct rfd rfdring[RFD_COUNT];
// } the_e100;

// // Public Functions
// //int nic_e100_enable(struct pci_func *);
// //int nic_e100_trans_pkt(void *, uint32_t);
// //int nic_e100_recv_pkt(void *);

// int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

// void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
// void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length);

// // TCB Command in TCB structure
// #define TCBCMD_NOP    0x0000
// #define TCBCMD_IND_ADD_SETUP  0x0001
// #define TCBCMD_CONFIGURE  0x0002
// #define TCBCMD_MUL_ADD_SETUP  0x0003
// #define TCBCMD_TRANSMIT   0x0004
// #define TCBCMD_LD_MICROCODE 0x0005
// #define TCBCMD_DUMP   0x0006
// #define TCBCMD_DIAGNOSE   0x0007

// // Go into Idle state after this frame is processed
// #define TCBCMD_EL   0x8000 
// // Go into Suspended state after this frame is processed
// #define TCBCMD_S    0x4000

// // CB Status in CB structure
// #define CBSTS_C     0x8000
// #define CBSTS_OK    0x2000

// // SCB Command

// #define SCBCMD_CU_NOP     0x0000
// #define SCBCMD_CU_START     0x0010
// #define SCBCMD_CU_RESUME    0x0020
// #define SCBCMD_CU_LOAD_COUNTER_ADD  0x0040
// #define SCBCMD_CU_DUMP_STAT_COUNTER 0x0050
// #define SCBCMD_CU_LOAD_BASE   0x0060
// #define SCBCMD_CU_DUMP_RESET_COUNTER  0x0070
// #define SCBCMD_CU_STATIC_RESUME   0x00a0

// #define SCBCMD_RU_NOP   0x0000
// #define SCBCMD_RU_START   0x0001
// #define SCBCMD_RU_RESUME  0x0002
// #define SCBCMD_RU_RDR   0x0003
// #define SCBCMD_RU_ABORT   0x0004
// #define SCBCMD_RU_LOAD_HDS  0x0005
// #define SCBCMD_RU_LOAD_BASE 0x0006

// // Enable Interrupts in SCB Command
// #define SCBINT_CX   0x8000  // CU interrupts when an action completed
// #define SCBINT_FR   0x4000  // RU interrupts when a frame is received
// #define SCBINT_CNA    0x2000  // CU interrupts when its status changed
// #define SCBINT_RNR    0x1000  // RU is not ready
// #define SCBINT_ER   0x0800  // Same with FR
// #define SCBINT_FCP    0x0400  // A flow control pause frame
// #define SCBINT_SI   0x0200  // For software interrupt
// #define SCBINT_M    0x0100  // Interrupt mask bit


// // SCB Status

// #define SCBSTS_CU_IDLE    0x00
// #define SCBSTS_CU_SUSP    0x40
// #define SCBSTS_CU_LPQA    0x80
// #define SCBSTS_CU_HQPA    0xc0

// #define SCBSTS_CU_MASK    0xC0

// #define SCBSTS_RU_IDLE    0x00
// #define SCBSTS_RU_SUSP    0x04
// #define SCBSTS_RU_NORES   0x08
// #define SCBSTS_RU_READY   0x10

// #define SCBSTS_RU_MASK    0x3C

// // RFD field
// #define RFD_EOF     0x8000
// #define RFD_F     0x4000
// #define RFD_LEN_MASK    0x3FFF

//>>>>>>>>>>>>>>>>>
// #define SHM_LENGTH  32

// #define SCB_STATUS_CU_MASK  (3 << 6)
// #define CU_IDLE     (0 << 6)
// #define CU_SUSPEND    (1 << 6)
// #define CU_LPQ      (2 << 6)
// #define CU_HQP      (3 << 6)

// #define SCB_STATUS_RU_MASK  (0xF << 2)
// #define RU_IDLE     (0 << 2)
// #define RU_SUSPEND    (1 << 2)
// #define RU_NO_RESOURCE    (2 << 2)
// #define RU_READY    (4 << 2)

// #define SCB_CMD_CU_START  (1 << 4)
// #define SCB_CMD_CU_RESUME (2 << 4)
// #define SCB_CMD_RU_START  (1)
// #define SCB_CMD_RU_RESUME (2)

// #define CB_STATUS_C (1 << 15)
// #define CB_STATUS_OK  (1 << 13)
// #define CB_CONTROL_EL (1 << 15)
// #define CB_CONTROL_S  (1 << 14)
// #define CB_CONTROL_I  (1 << 13)
// #define CB_CONTROL_NOP  (0)
// #define CB_CONTROL_TX (4)
// #define RFD_STATUS_C  (1 << 15)
// #define RFD_STATUS_OK (1 << 13)
// #define RFD_CONTROL_EL  (1 << 15)
// #define RFD_CONTROL_S (1 << 14)
// #define RFD_CONTROL_H (1 << 4)
// #define RFD_CONTROL_SF  (1 << 3)
// #define RFD_FLAGS_EOF (1 << 15)
// #define RFD_FLAGS_F (1 << 14)
// #define RFD_SIZE_MASK ((1 << 14) - 1)

// #define ETH_PACK_SIZE 1518

// // int pci_attach_e100(struct pci_func *pcif);

// // int e100_send_data(void *data, size_t count);
// // int e100_recv_data(void *data, size_t count);

// int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

// void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
// void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length);

#endif
