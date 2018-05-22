/**
 * author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 * Patterned after https://github.com/wh5a/jos/blob/master/kern/e100.c
 * The network device is different from what is used here. device e1000 from
 * qemu(what we use) gives product id =0x100e = 82540EM Gigabit Ethernet
 * Controller but the referenced code uses 0x1209 = 8255xER/82551IT Fast
 * Ethernet Controller which is device i82550 in qemu
 */

#include "e1000.h"
#include "defs.h"
#include "x86.h"
#include "arp_frame.h"
#include "nic.h"
#include "memlayout.h"

static void e1000_reg_write(uint32_t reg_addr, uint32_t value, struct e1000 *the_e1000) {
  *(uint32_t*)(the_e1000->membase + (reg_addr)) = value;
}

static uint32_t e1000_reg_read(uint32_t reg_addr, struct e1000 *the_e1000)
{
  uint32_t value = *(uint32_t*)(the_e1000->membase + reg_addr);
  //cprintf("Read value 0x%x from E1000 I/O port 0x%x\n", value, reg_addr);

  return value;

  //return the_e1000->membase+reg_addr/4;
}

// Each inb of port 0x84 takes about 1.25us
// Super stupid delay logic. Don't even know if this works
// or understand what port 0x84 does.
// Could not find an explanantion.
static void udelay(unsigned int u)
{
	unsigned int i;
	for (i = 0; i < u; i++)
		inb(0x84);
}

void e1000_send(void *driver, uint8_t *pkt, uint16_t length )
{
  struct e1000 *e1000 = (struct e1000*)driver;
  cprintf("e1000 driver: Sending packet of length:0x%x %x starting at physical address:0x%x\n", length, sizeof(struct ethr_hdr), V2P(e1000->tx_buf[e1000->tbd_tail]));
  memset(e1000->tbd[e1000->tbd_tail], 0, sizeof(struct e1000_tbd));
  memmove((e1000->tx_buf[e1000->tbd_tail]), pkt, length);
  e1000->tbd[e1000->tbd_tail]->addr = (uint64_t)(uint32_t)V2P(e1000->tx_buf[e1000->tbd_tail]);
	e1000->tbd[e1000->tbd_tail]->length = length;
	e1000->tbd[e1000->tbd_tail]->cmd = 9;//(E1000_TDESC_CMD_RS | E1000_TDESC_CMD_EOP | E1000_TDESC_CMD_IFCS);
  e1000->tbd[e1000->tbd_tail]->cso = 0;
	// update the tail so the hardware knows it's ready
	int oldtail = e1000->tbd_tail;
	e1000->tbd_tail = (e1000->tbd_tail + 1) % E1000_TBD_SLOTS;
	e1000_reg_write(E1000_TDT, e1000->tbd_tail, e1000);

	while( !E1000_TDESC_STATUS_DONE(e1000->tbd[oldtail]->status) )
	{
		udelay(2);
	}
  cprintf("after while loop\n");
}

int e1000_init(struct pci_func *pcif, void** driver, uint8_t *mac_addr) {
  struct e1000 *the_e1000 = (struct e1000*)kalloc();

	for (int i = 0; i < 6; i++) {
    // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
    if (pcif->reg_base[i] <= 0xffff) {
      the_e1000->iobase = pcif->reg_base[i];
      if(pcif->reg_size[i] != 64) {  // CSR is 64-byte
        panic("I/O space BAR size != 64");
      }
      break;
    } else if (pcif->reg_base[i] > 0) {
      the_e1000->membase = pcif->reg_base[i];
      if(pcif->reg_size[i] != (1<<17)) {  // CSR is 64-byte
        panic("Mem space BAR size != 128KB");
      }
    }
  }
  if (!the_e1000->iobase)
    panic("Fail to find a valid I/O port base for E1000.");
  if (!the_e1000->membase)
    panic("Fail to find a valid Mem I/O base for E1000.");

	the_e1000->irq_line = pcif->irq_line;
  the_e1000->irq_pin = pcif->irq_pin;
  cprintf("e1000 init: interrupt pin=%d and line:%d\n",the_e1000->irq_pin,the_e1000->irq_line);
  the_e1000->tbd_head = the_e1000->tbd_tail = 0;
  the_e1000->rbd_head = the_e1000->rbd_tail = 0;

  // Reset device but keep the PCI config
  // e1000_reg_write(E1000_CNTRL_REG,
  //   e1000_reg_read(E1000_CNTRL_REG, the_e1000) | E1000_CNTRL_RST_MASK,
  //   the_e1000);
  // //read back the value after approx 1us to check RST bit cleared
  // do {
  //   udelay(3);
  // }while(E1000_CNTRL_RST_BIT(e1000_reg_read(E1000_CNTRL_REG, the_e1000)));

  // //the manual says in Section 14.3 General Config -
  // //Must set the ASDE and SLU(bit 5 and 6(0 based index)) in the CNTRL Reg to allow auto speed
  // //detection after RESET
  // uint32_t cntrl_reg = e1000_reg_read(E1000_CNTRL_REG, the_e1000);
  // e1000_reg_write(E1000_CNTRL_REG, cntrl_reg | E1000_CNTRL_ASDE_MASK | E1000_CNTRL_SLU_MASK,
  //   the_e1000);

  //Read Hardware(MAC) address from the device
  uint32_t macaddr_l = e1000_reg_read(E1000_RCV_RAL0, the_e1000);
  uint32_t macaddr_h = e1000_reg_read(E1000_RCV_RAH0, the_e1000);
  *(uint32_t*)the_e1000->mac_addr = macaddr_l;
  *(uint16_t*)(&the_e1000->mac_addr[4]) = (uint16_t)macaddr_h;
  *(uint32_t*)mac_addr = macaddr_l;
  *(uint32_t*)(&mac_addr[4]) = (uint16_t)macaddr_h;
  char mac_str[18];
  unpack_mac(the_e1000->mac_addr, mac_str);
  mac_str[17] = 0;

  cprintf("\nMAC address of the e1000 device:%s\n", mac_str);


  //Transmit/Receive and DMA config beyond this point...
  //sizeof(tbd)=128bits/16bytes. so 256 of these will fit in a page of size 4KB.
  //But the struct e1000 has to contain pointer to these many descriptors.
  //Each pointer being 4 bytes, and 4 such array of pointers(inclusing packet buffers)
  //you get N*16+(some more values in the struct e1000) = 4096
  // N=128=E1000_TBD_SLOTS. i.e., the maximum number of descriptors in one ring
  struct e1000_tbd *ttmp = (struct e1000_tbd*)kalloc();
  for(int i=0;i<E1000_TBD_SLOTS;i++, ttmp++) {
    the_e1000->tbd[i] = (struct e1000_tbd*)ttmp;
    the_e1000->tbd[i]->addr = 0;
    the_e1000->tbd[i]->length=0;
    the_e1000->tbd[i]->cso=0;
    the_e1000->tbd[i]->cmd=(E1000_TDESC_CMD_RS>>24);
    the_e1000->tbd[i]->status = E1000_TXD_STAT_DD;
    the_e1000->tbd[i]->css=0;
    the_e1000->tbd[i]->special=0;
  }
  //check the last nibble of the transmit/receive rings to make sure they
  //are on paragraph boundary
  if( (V2P(the_e1000->tbd[0]) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Transmit Descriptor Ring not on paragraph boundary\n");
    kfree((char*)ttmp);
    return -1;
  }
  //same for rbd
  struct e1000_rbd *rtmp = (struct e1000_rbd*)kalloc();
  for(int i=0;i<E1000_RBD_SLOTS;i++, rtmp++) {
    the_e1000->rbd[i] = (struct e1000_rbd*)rtmp;
    the_e1000->rbd[i]->status = E1000_RXD_STAT_DD;
  }
  if( (V2P(the_e1000->rbd[0]) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Receive Descriptor Ring not on paragraph boundary\n");
    kfree((char*)rtmp);
    return -1;
  }

  //Now for the packet buffers in Receive Ring. Can fit 2 packet buf in 1 page
  struct packet_buf *tmp;

  for(int i=0; i<E1000_TBD_SLOTS; i+=2) {
    tmp = (struct packet_buf*)kalloc();
    the_e1000->tx_buf[i] = tmp;
    tmp++;
    //the_e1000->tbd[i]->addr = (uint32_t)the_e1000->tx_buf[i];
    //the_e1000->tbd[i]->addr_h = 0;
    the_e1000->tx_buf[i+1] = tmp;
    //the_e1000->tbd[i+1]->addr_l = (uint32_t)the_e1000->tx_buf[i+1];
    //the_e1000->tbd[i+1]->addr_h = 0;
  }

  //Write the Descriptor ring addresses in TDBAL, and RDBAL, plus HEAD and TAIL pointers
  e1000_reg_write(E1000_TDBAL, V2P(the_e1000->tbd[0]), the_e1000);
  e1000_reg_write(E1000_TDBAH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_TDLEN, (E1000_TBD_SLOTS*16), the_e1000);
  e1000_reg_write(E1000_TDH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_TCTL, //0x0004010A,
                  E1000_TCTL_EN |
                    E1000_TCTL_PSP |
                    E1000_TCTL_CT_SET(0x0f) |
                    E1000_TCTL_COLD_SET(0x200),
                  the_e1000);
  e1000_reg_write(E1000_TDT, 0, the_e1000);
  e1000_reg_write(E1000_TIPG, //0x60100a,
                  E1000_TIPG_IPGT_SET(10) |
                    E1000_TIPG_IPGR1_SET(10) |
                    E1000_TIPG_IPGR2_SET(10),
                  the_e1000);

  the_e1000->tbd_tail=the_e1000->tbd_head=0;
  the_e1000->rbd_tail=E1000_RBD_SLOTS-1;
  the_e1000->rbd_head=0;
                  
  e1000_reg_write(E1000_RCV_RAL0, 0x12005452, the_e1000);
  e1000_reg_write(E1000_RCV_RAH0, 0x5634|0x80000000, the_e1000);
  e1000_reg_write(E1000_MTA,0,the_e1000);
  e1000_reg_write(E1000_RDBAL, V2P(the_e1000->rbd[0]), the_e1000);
  e1000_reg_write(E1000_RDBAH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_RDLEN, (E1000_RBD_SLOTS*16), the_e1000);

  for(int i=0; i<E1000_RBD_SLOTS; i+=1) {
    tmp = (struct packet_buf*)kalloc();
    the_e1000->rx_buf[i] = tmp;
    //tmp++;
    the_e1000->rbd[i]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i])+4;
    the_e1000->rbd[i]->addr_h = 0;
    //the_e1000->rbd[i]->addr=(uint64_t)V2P((uint32_t)the_e1000->rx_buf[i]+4);
    //the_e1000->rx_buf[i+1] = tmp;
    //the_e1000->rbd[i+1]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i+1])+4;
    //the_e1000->rbd[i+1]->addr_h = 0;
  }

  e1000_reg_write(E1000_RDT, E1000_RBD_SLOTS-1, the_e1000);
  e1000_reg_write(E1000_RDH, 0x00000000, the_e1000);

  //e1000_reg_write(E1000_MANC,E1000_MANC_ARP_EN|E1000_MANC_ARP_RES_EN,the_e1000);


  //enable interrupts
  // e1000_reg_write(E1000_IMS, E1000_IMS_RXSEQ | 
  //                            E1000_IMS_RXO | 
  //                            E1000_IMS_RXT0|
  //                            E1000_IMS_TXQE|
  //                            E1000_IMS_LSC|
  //                            E1000_IMS_RXDMT0, the_e1000);
  //Receive control Register.
  uint32_t rflag=0;
  rflag|=E1000_RCTL_EN;
  rflag&=(~0x00000C00);
  //rflag|=E1000_RCTL_UPE;
  //rflag|=E1000_RCTL_LBM_MAC|E1000_RCTL_LBM_SLP|E1000_RCTL_LBM_TCVR;
  //rflag|=E1000_RCTL_VFE;
  rflag|=E1000_RCTL_BAM;
  rflag|=0x00000000;
  rflag|=E1000_RCTL_SECRC;
  e1000_reg_write(E1000_RCTL,rflag,the_e1000);
  
//                E1000_RCTL_EN |
//                  E1000_RCTL_BAM |
//                  E1000_RCTL_BSIZE | 0x00000008,//|
                //  E1000_RCTL_SECRC,
//                the_e1000);
cprintf("e1000:Interrupt enabled mask:0x%x\n", e1000_reg_read(E1000_IMS, the_e1000));
  //Register interrupt handler here...
  picenable(the_e1000->irq_line);
  ioapicenable(the_e1000->irq_line, 0);
  ioapicenable(the_e1000->irq_line, 1);


  *driver = the_e1000;
  return 0;
}

void e1000_recv(void *driver, uint8_t* pkt, uint16_t *length) {
  struct e1000 *the_e1000=(struct e1000*)driver;
  int i=(the_e1000->rbd_tail+1)%E1000_RBD_SLOTS;
  if(!(the_e1000->rbd[i]->status&E1000_RXD_STAT_DD )||!(the_e1000->rbd[i]->status&E1000_RXD_STAT_EOP))
  {
    *length=0;
    return;
  }
  *length=the_e1000->rbd[i]->length;
  //pkt=&(the_e1000->rx_buf[i]->buf[0]);
  memmove(pkt,(uint8_t*)P2V(the_e1000->rbd[i]->addr_l),(uint)(*length));
  the_e1000->rbd[i]->status=0;
  cprintf("ERRORS: %x\n",the_e1000->rbd[i]->errors);
  the_e1000->rbd_tail=i;
}

///>>>>>>>>>>>>>>>>>>>>>>>>>>

// #include "e1000.h"
// #include "defs.h"
// #include "x86.h"
// #include "pci.h"
// #include "nic.h"
// #include "memlayout.h"


// struct e100 the_e100;

// // Each inb of port 0x84 takes about 1.25us
// void udelay(unsigned int u)
// {
//   unsigned int i;
//   for (i = 0; i < u; i++)
//     inb(0x84);
// }

// static void
// e100_scb_wait(void)
// {
//   int i;

//   for (i = 0; i < 100000; i++)
//     if (inb(the_e100.iobase + E100_CSR_SCB_COMMAND) == 0)
//       return;
  
//   cprintf("e100_scb_wait: timeout\n");
// }

// static void
// e100_scb_cmd(uint8_t cmd)
// {
//   outb(the_e100.iobase + E100_CSR_SCB_COMMAND, cmd);
// }

// static void e100_tx_start(void)
// {
//   int i = the_e100.tx_tail % E100_TX_SLOTS;

//   if (the_e100.tx_tail == the_e100.tx_head)
//     panic("oops, no TCBs");

//   if (the_e100.tx_idle) {
//     e100_scb_wait();
//     outl(the_e100.iobase + E100_CSR_SCB_GENERAL, 
//          V2P(&the_e100.tx[i].tcb));
//     e100_scb_cmd(E100_SCB_COMMAND_CU_START);
//     the_e100.tx_idle = 0;
//   } else {
//     e100_scb_wait();
//     e100_scb_cmd(E100_SCB_COMMAND_CU_RESUME);
//   }
// }

// static void e100_rx_start(void)
// {
//   int i = the_e100.rx_tail % E100_RX_SLOTS;

//   if (the_e100.rx_tail == the_e100.rx_head)
//     panic("oops, no RFDs");

//   if (the_e100.rx_idle) {
//     e100_scb_wait();
//     outl(the_e100.iobase + E100_CSR_SCB_GENERAL, 
//          V2P(&the_e100.rx[i].rfd));
//     e100_scb_cmd(E100_SCB_COMMAND_RU_START);
//     the_e100.rx_idle = 0;
//   } else {
//     e100_scb_wait();
//     e100_scb_cmd(E100_SCB_COMMAND_RU_RESUME);
//   }
// }

// void e1000_recv(void *driver, uint8_t* pkt, uint16_t *length)
// {
//   int i;

//   if (the_e100.rx_head - the_e100.rx_tail == E100_TX_SLOTS) {
//     cprintf("e100_rxbuf: no space\n");
//     return;
//   }

//   i = the_e100.rx_head % E100_TX_SLOTS;

//   // The first 4 bytes will hold the number of bytes recieved
//   the_e100.rx[i].rbd.rbd_buffer = V2P(pkt) + 4;
//   the_e100.rx[i].rbd.rbd_size = (4096 - 4) & E100_SIZE_MASK;
//   the_e100.rx[i].rfd.rfa_status = 0;
//   the_e100.rx[i].rfd.rfa_control = 
//     E100_RFA_CONTROL_SF | E100_RFA_CONTROL_S;

//   the_e100.rx[i].p = pkt;
//   //the_e100.rx[i].offset = offset;
//   the_e100.rx_head++;

//   e100_rx_start();

// }

// void e1000_send(void *driver, uint8_t *pkt, uint16_t length)
// {
//   int i;

//   struct e100 *the_e100=(struct e100*) driver;

//   if (the_e100->tx_head - the_e100->tx_tail == E100_TX_SLOTS) {
//     cprintf("e100_txbuf: no space\n");
//     return;
//   }

//   i = the_e100->tx_head % E100_TX_SLOTS;

//   memmove((the_e100->tx[i].p), pkt, length);

//   the_e100->tx[i].tbd.tb_addr = V2P(the_e100->tx[i].p);
//   the_e100->tx[i].tbd.tb_size = length & E100_SIZE_MASK;
//   the_e100->tx[i].tcb.cb_status = 0;
//   the_e100->tx[i].tcb.cb_command = E100_CB_COMMAND_XMIT |
//     E100_CB_COMMAND_SF | E100_CB_COMMAND_I | E100_CB_COMMAND_S;

//   the_e100->tx_head++;
  
//   e100_tx_start();
// }

// static void e100_intr_tx(void)
// {
//   int i;

//   for (; the_e100.tx_head != the_e100.tx_tail; the_e100.tx_tail++) {
//     i = the_e100.tx_tail % E100_TX_SLOTS;
    
//     if (!(the_e100.tx[i].tcb.cb_status & E100_CB_STATUS_C))
//       break;

//     //page_decref(the_e100.tx[i].p);
//     //the_e100.tx[i].p = 0;
//   }
// }

// static void e100_intr_rx(void)
// {
//   //int *count;
//   int i;

//   for (; the_e100.rx_head != the_e100.rx_tail; the_e100.rx_tail++) {
//     i = the_e100.rx_tail % E100_RX_SLOTS;
    
//     if (!(the_e100.rx[i].rfd.rfa_status & E100_RFA_STATUS_C))
//       break;

//     // count = (int*)(V2P(the_e100.rx[i].p) + the_e100.rx[i].offset);
//     // if (the_e100.rx[i].rfd.rfa_status & E100_RFA_STATUS_OK)
//     //   *count = the_e100.rx[i].rbd.rbd_count & E100_SIZE_MASK;
//     // else
//     //   *count = -1;

//     //page_decref(the_e100.rx[i].p);
//     the_e100.rx[i].p = 0;
//     the_e100.rx[i].offset = 0;
//   }
// }

// void e100_intr(void)
// {
//   int r;
  
//   r = inb(the_e100.iobase + E100_CSR_SCB_STATACK);
//   outb(the_e100.iobase + E100_CSR_SCB_STATACK, r);
  
//   if (r & (E100_SCB_STATACK_CXTNO | E100_SCB_STATACK_CNA)) {
//     r &= ~(E100_SCB_STATACK_CXTNO | E100_SCB_STATACK_CNA);
//     e100_intr_tx();
//   }

//   if (r & E100_SCB_STATACK_FR) {
//     r &= ~E100_SCB_STATACK_FR;
//     e100_intr_rx();
//   }

//   if (r & E100_SCB_STATACK_RNR) {
//     r &= ~E100_SCB_STATACK_RNR;
//     the_e100.rx_idle = 1;
//     e100_rx_start();
//     cprintf("e100_intr: RNR interrupt, no RX bufs?\n");
//   }

//   if (r)
//     cprintf("e100_intr: unhandled STAT/ACK %x\n", r);
// }

// int e1000_init(struct pci_func *pcif, void** driver, uint8_t *mac_addr)
// {
//   cprintf("initializing!\n");
//   *driver=&the_e100;
//   // Store parameters
//   int i;
//   for (i = 0; i < 6; i++) {
//           // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
//           if (pcif->reg_base[i] <= 0xffff) {
//             the_e100.iobase = pcif->reg_base[i];
//             //assert(pcif->reg_size[i] == 64);  // CSR is 64-byte
//             break;
//           }
//         }
//   if (!the_e100.iobase)
//           panic("Fail to find a valid I/O port base for E100.");
//   e100_irq = pcif->irq_line;

//   // Reset device by writing the PORT DWORD
//   outl(the_e100.iobase + E100_CSR_PORT, E100_PORT_SOFTWARE_RESET);
//   udelay(10);

//   the_e100.tx_idle = 1;
//   the_e100.rx_idle = 1;
//   int next;
//   // Setup TX DMA ring for CU
//   for (i = 0; i < E100_TX_SLOTS; i++) {
//     next = (i + 1) % E100_TX_SLOTS;
//     memset(&the_e100.tx[i], 0, sizeof(the_e100.tx[i]));
//     the_e100.tx[i].tcb.link_addr = V2P(&the_e100.tx[next].tcb);
//     the_e100.tx[i].tcb.tbd_array_addr = V2P(&the_e100.tx[i].tbd);
//     the_e100.tx[i].tcb.tbd_number = 1;
//     the_e100.tx[i].tcb.tx_threshold = 4;
//     the_e100.tx[i].p=(uint8_t*)kalloc();
//   }
//   the_e100.tx_head=the_e100.tx_tail=0;

//   // Setup RX DMA ring for RU
//   for (i = 0; i < E100_RX_SLOTS; i++) {
//     next = (i + 1) % E100_RX_SLOTS;
//     memset(&the_e100.rx[i], 0, sizeof(the_e100.rx[i]));
//     the_e100.rx[i].rfd.link_addr = V2P(&the_e100.rx[next].rfd);
//     the_e100.rx[i].rfd.rbd_addr = V2P(&the_e100.rx[i].rbd);
//     the_e100.rx[i].rbd.rbd_link = V2P(&the_e100.rx[next].rbd);
//     the_e100.rx[i].p=(uint8_t*)kalloc();
//   }

//   picenable(e100_irq);
//   ioapicenable(e100_irq, 0);
//   ioapicenable(e100_irq, 1);


//   return 0;
// }

///>>>>>>>>>>>>>>>>>>>>>>>>

// #include "e1000.h"
// #include "defs.h"
// #include "x86.h"
// #include "pci.h"
// #include "nic.h"
// #include "memlayout.h"
// // #include "e1000.h"
// // #include "defs.h"
// // #include "x86.h"
// // #include "pci.h"
// // #include "nic.h"
// // #include "memlayout.h"


// #define PADDR V2P

// uint32_t csr_port;

// void *cu_base;
// void *ru_base;

// struct tcb *cu_ptr; // pointer where to put the next transmit packet
// struct rfd *ru_ptr;

// static struct rfd *pre_ru_ptr;

// int debug = 1;

// static int
// nic_alloc_cbl(void)
// {
//   int i;
//   struct tcb *cb_p=&the_e100.tcbring[0];
//   /* tcb_size is 1518+8+8=1534 bytes
//    * However, because we use 32-bit machine, alignment
//    * is required. Here we use 4 bytes alignment
//    */
//   //uint32_t tcb_size = ROUNDUP(sizeof(struct tcb), 4);

//   // Use the flash memory, it is 16K, the physical memory is 4K
//   //cu_base = (void *)(pcircd.reg_base[2]);

//   // Set CU_ptr to cu_base for further transmission
//   cu_ptr = cb_p ;//= (struct tcb *)cu_base;
//   cu_base = cb_p;

//   // Construct the DMA TX Ring
//   for (i = 0; i < TCB_COUNT - 1; i ++) {
//     cb_p->cb.cmd = 0;
//     cb_p->cb.status = 0;
//     cb_p->cb.link = /*tcb_size **/ (i + 1);
//     cb_p++;
//   }
//   // Finish the Ring
//   cb_p->cb.link = 0;
//   return 0;
// }

// static int
// nic_alloc_rfa(void)
// {
//   int i;
//   struct rfd *cb_p=&the_e100.rfdring[0];
//   /* rfd_size is 1518+8+8=1534 bytes
//    * However, because we use 32-bit machine, alignment
//    * is required. Here we use 4 bytes alignment
//    */
//   //uint32_t rfd_size = ROUNDUP(sizeof(struct rfd), 4);
//   //uint32_t tcb_size = ROUNDUP(sizeof(struct tcb), 4);

//   // Use the flash memory, set it to the end of the area
//   // which has been taken by tcb ring
//   //ru_base = (void *)(pcircd.reg_base[2] + tcb_size * TCB_COUNT);

//   // Set ru_ptr to ru_base for further recepted processing
//   ru_ptr = cb_p ;//= (struct rfd *)ru_base;
//   ru_base = cb_p;

//   // Construct the DMA RX Ring
//   for (i = 0; i < RFD_COUNT - 1; i ++) {
//     cb_p->cb.cmd = 0;
//     cb_p->cb.status = 0;
//     cb_p->cb.link = /*rfd_size **/ (i + 1);
//     cb_p->reserved = 0xFFFFFFFF;
//     cb_p->buffer_size = MAX_ETH_FRAME;
//     cb_p++;
//   }
//   // Finish the Ring
//   cb_p->cb.link = 0;
//   cb_p->cb.cmd |= TCBCMD_EL;
  
//   pre_ru_ptr = cb_p;

//   return 0;
// }

// int
// nic_e100_enable(struct pci_func *pcif)
// {
//   int i;

//   //pci_func_enable(pcif);

//   // Record the Memory and I/O information
//   for (i = 0; i < 6; i ++) {
//     pcircd.reg_base[i] = pcif->reg_base[i];
//     pcircd.reg_size[i] = pcif->reg_size[i];
//   }
//   // Record the IRQ Number to receive interrupts from device
//   pcircd.irq_line = pcif->irq_line;

//   csr_port = pcif->reg_base[1];

//   // reg_base[0] is the CSR Memory Mapped Base Address Register
//   // reg_base[1] is the CSR I/O Mapped Base Address Register
//   // reg_base[2] is the Flash Memory Mapped Base Address Register
  
//   // Reset the NIC card by writing 0x0000 into the PORT area in CSR
//   outl(csr_port + 0x8, 0x0);

//   // Alloc the TCB Ring
//   nic_alloc_cbl();

//   // Load CU base
//   // First write the General Pointer field in SCB
//   outl(csr_port + 0x4, PADDR((uint32_t)cu_base));
//   // Second, write the SCB command to load the pointer
//   outw(csr_port + 0x2, SCBCMD_CU_LOAD_BASE);
  
//   // Alloc the RFA
//   nic_alloc_rfa();

//   if (debug)
//     cprintf("DBG: ru_base = %x, irq_line = %d\n", ru_base, pcircd.irq_line);
//   // Load RU base
//   // First write the General Pointer field in SCB
//   outl(csr_port + 0x4, PADDR((uint32_t)ru_base));
//   // Second, write the SCB command to load the pointer
//   outw(csr_port + 0x2, SCBCMD_RU_LOAD_BASE);

//   // Start RU, start receiving packets
//   // FIXME: Why I cannot use RU_START?
//   outw(csr_port + 0x2, SCBCMD_RU_RESUME);
  
//   return 0;
// }

// int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr)
// {
//   return nic_e100_enable(pcif);
//   *driver=0;
// }

// // Before we transmit a packet, we first reclaim any buffers which 
// // have been marked as transmitted by the E100. But we do not actually
// // reclaim it, just make a mark since what we use is a ring buffer
// static void
// tx_reclaim_transmited_buffers(struct tcb *tcb_ptr)
// {
//   int i;
//   for (i = 0; i < TCB_COUNT; i ++, tcb_ptr = (cu_base + tcb_ptr->cb.link)) {
//     if ((tcb_ptr->cb.status & CBSTS_C) && (tcb_ptr->cb.status & CBSTS_OK)) {
//       // Completed Successful

//       // Clear the bit
//       tcb_ptr->cb.status = 0;
//       tcb_ptr->cb.cmd = 0;
//     } else if((tcb_ptr->cb.status & CBSTS_C) && !(tcb_ptr->cb.status & CBSTS_OK)) {
//       // Error Occured, just reuse this block
//       cprintf("One Frame Transmit Error! : %d\n", i);
//       tcb_ptr->cb.status = 0;
//       tcb_ptr->cb.cmd = 0;
//     }
//   }
// }

// // If the tcb block can be used by the driver to transmit packet
// static int
// tcb_avail(struct tcb *tcb_ptr)
// {
//   return !(tcb_ptr->cb.cmd);
// }

// static void
// construct_tcb_header(struct tcb *tcb_ptr, uint16_t datalen)
// {
//   tcb_ptr->cb.cmd = TCBCMD_TRANSMIT | TCBCMD_S;
//   tcb_ptr->cb.status = 0;
//   tcb_ptr->tbd_array_addr = 0xFFFFFFFF;
//   tcb_ptr->tbd_thrs = 0xE0;
//   tcb_ptr->tbd_byte_count = datalen;

//   if (debug)
//     cprintf("DBG: Construct a tcb header : datalen = %d, this ptr = %x, next = %x\n", 
//       tcb_ptr->tbd_byte_count, tcb_ptr, tcb_ptr->cb.link);
// }

// int
// nic_e100_trans_pkt(void *pkt_data, uint32_t datalen)
// {
//   if (debug)
//     cprintf("DBG: Transmit a packet, datalen = %d\n", datalen);

//   // First, reclaim any buffer that is marked transmitted by E100
//   tx_reclaim_transmited_buffers(cu_ptr);

//   if (!tcb_avail(cu_ptr)) {
//     // There is no available slot in the DMA transmit ring
//     // Drop the packet
  
//     if (debug)
//       cprintf("DBG: No slot in DMA ring available, Drop the packet\n");

//     return 0;
//   }

//   // Put the pkt_data into the available slot
//   construct_tcb_header(cu_ptr, (uint16_t)datalen);
//   // Copy the data into the
//   memmove(cu_ptr->pkt_data, pkt_data, datalen);

//   uint8_t cu_status = SCBSTS_CU_MASK & inb(csr_port + 0x0);

//   if (cu_status == SCBSTS_CU_IDLE) {
//   // Start CU if it is idle, CU is not associated with a CB in the CBL
  
//     if (debug)
//       cprintf("DBG: CU is Idle, Start it\n");

//     // Resume the CU
//     // FIXME: Cannot use CU_START command here, why?
// //    outw(csr_port + 0x2, SCBCMD_CU_RESUME);

// // #if 0
// //     // Start the CU
//      outw(csr_port + 0x2, SCBCMD_CU_START);
// // #endif
//   } else if (cu_status == SCBSTS_CU_SUSP) {
//   // Resume CU if it is suspended, CU has read the next link in the CBL
  
//     if (debug)
//       cprintf("DBG: CU is Suspended, Resume it\n");

//     // Resume the CU
//     outw(csr_port + 0x2, SCBCMD_CU_RESUME);
//   }
//   // else, CU is working, we leave her alone =)

//   // Update cu_ptr to point to next frame, so we can use it when we transmit
//   // a packet next time
//   cu_ptr = cu_base + cu_ptr->cb.link;

//   cprintf("here");

//   return 0;
// }

// void e1000_send(void *e1000, uint8_t* pkt, uint16_t length)
// {
//   nic_e100_trans_pkt(pkt,(uint32_t)length);
// }

// static int
// rfd_avail(struct rfd *rfd_ptr)
// {
//   if ((rfd_ptr->cb.status & CBSTS_C) && (rfd_ptr->cb.status & CBSTS_OK)) {
//     // A packet is received in this buffer
//     if (debug)
//       cprintf("DBG: frame available in the rfa, wait...\n");
//     return 1;

//   } else if((rfd_ptr->cb.status & CBSTS_C) && !(rfd_ptr->cb.status & CBSTS_OK)) {
//     // Error Occured, just reuse this block
//     if (debug)
//       cprintf("DBG: One Frame Receive Error! : \n");
//     return 1;
//   }

//   return 0;
// }

// void
// nic_clear_rfd(struct rfd *rfd_ptr)
// {
//   rfd_ptr->cb.cmd = 0;
//   rfd_ptr->cb.status = 0;
//   rfd_ptr->reserved = 0xFFFFFFFF;
//   rfd_ptr->buffer_size = MAX_ETH_FRAME;
//   // Most Important: the EOF and F field must be set to 0 
//   // to reuse the rfd in the rfa
//   rfd_ptr->actual_count = 0;
//   rfd_ptr->cb.cmd |= TCBCMD_EL;
// }

// int
// nic_e100_recv_pkt(void *pkt_buf)
// {
//   uint8_t ru_status;
//   uint16_t pkt_actual_count;

//   if (!rfd_avail(ru_ptr)) {
//     // There is no available slot in the DMA rfa ring
  
//     if (debug)
//       cprintf("DBG: No frame available in the rfa, wait...\n");

//     return 0;
//   }

//   pkt_actual_count = RFD_LEN_MASK & ru_ptr->actual_count;
//   if (debug)
//     cprintf("DBG: A packet with length %d received\n", pkt_actual_count);

//   memmove(pkt_buf, ru_ptr->pkt_data, pkt_actual_count);

//   nic_clear_rfd(ru_ptr);

//   // Mark the previous RU as not suspend after packets received
//   pre_ru_ptr->cb.cmd &= ~TCBCMD_EL;
//   // Record the previous ru we write to clear
//   pre_ru_ptr = ru_ptr;
//   // Update ru_ptr to point to next frame, so we can use it when we receive next pkt.
//   ru_ptr = ru_base + ru_ptr->cb.link;

//   ru_status = SCBSTS_RU_MASK & inb(csr_port + 0x0);

//   if (ru_status == SCBSTS_RU_SUSP) {
//     // Resume RU, start receiving packets
//     outw(csr_port + 0x2, SCBCMD_RU_RESUME);
//   } else if (ru_status == SCBSTS_RU_IDLE) {
//     outw(csr_port + 0x2, SCBCMD_RU_RESUME);
//   }
//   // else, RU is working normally and receiving packets, leave her alone =)
  
//   return (int)pkt_actual_count;
// }

// void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length)
// {
//   *length=(uint16_t)nic_e100_recv_pkt(pkt);
// }
//>>>>>>>>>>>>>>>>>>>>>>>>
// static uint32_t e100_io_addr;
// static uint32_t e100_io_size;
// static uint8_t e100_irq_line;

// #define size_t uint16_t

// struct cb {
//   volatile uint16_t status;
//   uint16_t cmd;
//   uint32_t link;
//   uint32_t tbd;
//   uint16_t count;
//   uint16_t flags;
//   uint8_t data[ETH_PACK_SIZE];
//   struct cb *next;
//   struct cb *prev;
// } __attribute__((aligned(4), packed));

// struct rfd {
//   volatile uint16_t status;
//   uint16_t cmd;
//   uint32_t link;
//   uint32_t reserved;
//   volatile uint16_t count;
//   uint16_t capacity;
//   uint8_t data[ETH_PACK_SIZE];
//   struct rfd *next;
//   struct rfd *prev;
// } __attribute__((aligned(4), packed));

// static struct cb cbs[SHM_LENGTH];
// static struct cb *cb;

// static struct rfd rfds[SHM_LENGTH];
// static struct rfd *rfd;

// static void
// delay(void)
// {
//   inb(0x84);
//   inb(0x84);
//   inb(0x84);
//   inb(0x84);
// }

// static void
// shm_init(void)
// {
//   int i;

//   for (i = 0; i < SHM_LENGTH; i++) {
//     cbs[i].cmd = 0;
//     cbs[i].status = CB_STATUS_C;
//     cbs[i].next = cbs + (i + 1) % SHM_LENGTH;
//     cbs[i].prev = (i == 0) ? cbs + SHM_LENGTH - 1 : cbs + i - 1;
//     cbs[i].link = PADDR(cbs[i].next);
//   }
//   cbs[0].cmd = CB_CONTROL_NOP | CB_CONTROL_S;

//   outl(e100_io_addr + 4, PADDR(cbs));
//   outw(e100_io_addr + 2, SCB_CMD_CU_START);

//   cb = cbs + 1;

//   for (i = 0; i < SHM_LENGTH; i++) {
//     rfds[i].cmd = 0;
//     rfds[i].status = 0;
//     rfds[i].count = 0;
//     rfds[i].capacity = ETH_PACK_SIZE;
//     rfds[i].next = rfds + (i + 1) % SHM_LENGTH;
//     rfds[i].prev = (i == 0) ? rfds + SHM_LENGTH - 1 : rfds + i - 1;
//     rfds[i].link = PADDR(rfds[i].next);
//   }
//   rfds[SHM_LENGTH - 1].cmd = RFD_CONTROL_EL;

//   outl(e100_io_addr + 4, PADDR(rfds));
//   outw(e100_io_addr + 2, SCB_CMD_RU_START);

//   rfd = rfds;
// }

// int
// pci_attach_e100(struct pci_func *pcif)
// {
//   //int i;

//   //pci_func_enable(pcif);
//   e100_io_addr = pcif->reg_base[1];
//   e100_io_size = pcif->reg_base[1];
//   e100_irq_line = pcif->irq_line;

//   outl(e100_io_addr + 8, 0);
//   delay();
//   delay();
//   shm_init();

//   return 1;
// }

// int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr)
// {
//   pci_attach_e100(pcif);
//   return 0;
// }


// static int
// e100_pack_cb(void *data, size_t count)
// {
//   if (!(cb->status & CB_STATUS_C))
//     return 0;

//   if (count > ETH_PACK_SIZE)
//     count = ETH_PACK_SIZE;

//   cb->cmd = CB_CONTROL_S | CB_CONTROL_TX;
//   cb->status = 0;
//   cb->tbd = 0xffffffff;
//   cb->flags = 0xe0;
//   cb->count = count;
//   memmove(cb->data, data, count);

//   cb->prev->cmd &= ~CB_CONTROL_S;

//   return count;
// }

// int
// e100_send_data(void *data, size_t count)
// {
//   int ret, sent = 0;

//   while (sent != count) {
//     ret = e100_pack_cb(data + sent, count - sent);
//     if (ret == 0)
//       break;
//     sent += ret;
//     cb = cb->next;
//   }

//   if (sent != 0 && (inw(e100_io_addr) & SCB_STATUS_CU_MASK) == CU_SUSPEND)
//     outw(e100_io_addr + 2, SCB_CMD_CU_RESUME);

//   return sent;
// }


// void e1000_send(void *e1000, uint8_t* pkt, uint16_t length)
// {
//   e100_send_data(pkt,length);
// }

// static int
// e100_unpack_rfd(void *data, size_t count)
// {
//   if (!(rfd->status & RFD_STATUS_C))
//     return 0;

//   if (count >= (rfd->count & RFD_SIZE_MASK))
//     count = rfd->count & RFD_SIZE_MASK;
//   else
//     return 0;

//   memmove(data, rfd->data, count);

//   rfd->cmd = RFD_CONTROL_EL;
//   rfd->status = 0;
//   rfd->count = 0;
//   rfd->capacity = ETH_PACK_SIZE;

//   rfd->prev->cmd &= ~RFD_CONTROL_EL;

//   return count;
// }

// int
// e100_recv_data(void *data, size_t count)
// {
//   int ret;

//   ret = e100_unpack_rfd(data, count);
//   if (ret == 0)
//     return 0;

//   rfd = rfd->next;
//   if (ret != 0 && (inw(e100_io_addr) & SCB_STATUS_RU_MASK)
//       == RU_NO_RESOURCE)
//     outw(e100_io_addr + 2, SCB_CMD_RU_RESUME);

//   return ret;
// }

// void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length)
// {
//   *length=e100_recv_data(pkt,4096);
// }