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

   return value;
 }

 // Each inb of port 0x84 takes about 1.25us
 // Super stupid delay logic. Don't even know if this works
 // or understand what port 0x84 does.
 // Could not find an explanantion.
void udelay(unsigned int u)
 {
 	unsigned int i;
 	for (i = 0; i < u; i++)
 		inb(0x84);
 }

 void e1000_send(void *driver, uint8_t *pkt, uint16_t length )
 {
     cprintf("e1000 send:\n");
     int k;
     for(k=0;k!=length;++k)
     {
         if(k%12==0 && k) cprintf("\n");
         cprintf("%x%x ",((pkt[k])>>4)&(0xf),(pkt[k])&(0xf));
     }
     cprintf("\n");

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
       //cprintf("membase set: %d\n",i);
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
   //the_e1000->irq_pin = pcif->irq_pin;
   //cprintf("e1000 init: interrupt pin=%d and line:%d\n",the_e1000->irq_pin,the_e1000->irq_line);
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

   cprintf("MAC address of the e1000 device:%s\n", mac_str);


   //Transmit/Receive and DMA config beyond this point...
   //sizeof(tbd)=128bits/16bytes. so 256 of these will fit in a page of size 4KB.
   //But the struct e1000 has to contain pointer to these many descriptors.
   //Each pointer being 4 bytes, and 4 such array of pointers(inclusing packet buffers)
   //you get N*16+(some more values in the struct e1000) = 4096
   // N=128=E1000_TBD_SLOTS. i.e., the maximum number of descriptors in one ring
   struct e1000_tbd *ttmp = (struct e1000_tbd*)kalloc();
   for(int i=0;i<E1000_TBD_SLOTS;i++, ttmp++) {
     //cprintf("TBD[%d] add: %x\n",i,ttmp);
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
     the_e1000->rbd[i]->status = 0;//E1000_RXD_STAT_DD;
     the_e1000->rbd[i]->errors=0;
     the_e1000->rbd[i]->checksum=0;
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
   //e1000_reg_write(E1000_MTA,0,the_e1000);
   e1000_reg_write(E1000_RDBAL, V2P(*(the_e1000->rbd)), the_e1000);
   e1000_reg_write(E1000_RDBAH, 0x00000000, the_e1000);
   e1000_reg_write(E1000_RDLEN, (E1000_RBD_SLOTS*16), the_e1000);
   cprintf("RX Ring Size: %d\n",(E1000_RBD_SLOTS*16));

   for(int i=0; i<E1000_RBD_SLOTS; i+=1) {
     tmp = (struct packet_buf*)kalloc();
     the_e1000->rx_buf[i] = tmp;
     //tmp++;
     // the_e1000->rbd[i]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i])+4;
     // the_e1000->rbd[i]->addr_h = 0;
     // // cprintf("origin: %x\n",V2P((uint32_t)the_e1000->rx_buf[i]));
     // cprintf("after: %x\n",V2P((uint32_t)the_e1000->rx_buf[i])+4);
     the_e1000->rbd[i]->addr=(uint64_t)V2P((uint32_t)the_e1000->rx_buf[i]+4);
     //the_e1000->rx_buf[i+1] = tmp;
     //the_e1000->rbd[i+1]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i+1])+4;
     //the_e1000->rbd[i+1]->addr_h = 0;
   }

   e1000_reg_write(E1000_RDT, E1000_RBD_SLOTS-1, the_e1000);
   e1000_reg_write(E1000_RDH, 0x00000000, the_e1000);
   //e1000_reg_write(E1000_RDT, (uint32_t)P2V(&(the_e1000->rbd[E1000_RBD_SLOTS-2])), the_e1000);
   //e1000_reg_write(E1000_RDH, (uint32_t)P2V(&(the_e1000->rbd[0])), the_e1000);

   //cprintf("%x\n%x\n",E1000_RBD_SLOTS,(uint32_t)P2V(&(the_e1000->rbd[E1000_RBD_SLOTS-2])));
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
 //cprintf("e1000:Interrupt enabled mask:0x%x\n", e1000_reg_read(E1000_IMS, the_e1000));
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
   //memmove(pkt,(uint8_t*)P2V(the_e1000->rbd[i]->addr_l),(uint)(*length));
   memmove(pkt,P2V((uint8_t*)(uint32_t)(the_e1000->rbd[i]->addr)),(uint)(*length));
   the_e1000->rbd[i]->status=0;
   cprintf("ERRORS: %x\n",the_e1000->rbd[i]->errors);
   cprintf("CHECKSUM: %x\n",the_e1000->rbd[i]->checksum);
   the_e1000->rbd_tail=i;
 }