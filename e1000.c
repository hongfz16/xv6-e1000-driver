#include "e1000.h"
#include "defs.h"
#include "x86.h"
#include "arp_frame.h"
#include "nic.h"
#include "memlayout.h"

int pci_e1000_attach(struct pci_func *pcif){

    e1000=(uint32_t*)pcif->reg_base[0];

    for (int i=0; i<NTXDESC; i++) {
        tx_desc_table[i].addr=0;
        tx_desc_table[i].length=0;
        tx_desc_table[i].cso=0;
        tx_desc_table[i].cmd=(E1000_TXD_CMD_RS>>24);
        tx_desc_table[i].status=E1000_TXD_STAT_DD;
        tx_desc_table[i].css=0;
        tx_desc_table[i].special=0;
    }
    *E1000_REG(E1000_TDBAL)=V2P(tx_desc_table);
    *E1000_REG(E1000_TDBAH)=0;
    *E1000_REG(E1000_TDLEN)=sizeof(tx_desc_table);
    *E1000_REG(E1000_TDH)=0;
    *E1000_REG(E1000_TDT)=0;
    tdt=E1000_REG(E1000_TDT);
    
    uint32_t tctl=0x0004010A;
    *E1000_REG(E1000_TCTL)=tctl;
    uint32_t tpg=0;
    tpg=10;
    tpg|=4<<10;
    tpg|=6<<20;
    tpg&=0x3FFFFFFF;
    
    *E1000_REG(E1000_TIPG)=tpg;
    
    
    //receive
    *E1000_REG(E1000_RAL)=0x12005452;
    *E1000_REG(E1000_RAH)=0x5634|E1000_RAH_AV;
    *E1000_REG(E1000_RDBAL)=V2P(rx_desc_table);
    *E1000_REG(E1000_RDBAH)=0;
    *E1000_REG(E1000_RDLEN)=sizeof(rx_desc_table);
    for (int i=0; i<NRXDESC; i++) {
        rx_desc_table[i].addr=(uint64_t)(uint32_t)P2V((void*)kalloc())+4;
    }
    *E1000_REG(E1000_RDT)=NRXDESC-1;
    *E1000_REG(E1000_RDH)=0;
    rdt=E1000_REG(E1000_RDT);
    
    uint32_t rflag=0;
    rflag|=E1000_RCTL_EN;
    rflag&=(~E1000_RCTL_DTYP_MASK);
    rflag|=E1000_RCTL_BAM;
    rflag|=E1000_RCTL_SZ_2048;
    rflag|=E1000_RCTL_SECRC;
    *E1000_REG(E1000_RCTL)=rflag;
    
    return 0;
}

int e1000_put_tx_desc(struct tx_desc *td) {
    struct tx_desc *tail=tx_desc_table+(*tdt);
    if (!(tail->status&E1000_TXD_STAT_DD)) {
        return -1;
    }
    *tail=*td;
    *tdt=((*tdt)+1)&(NTXDESC-1);
    return 0;
}

int e1000_get_rx_desc(struct rx_desc *rd) {
    int i=(*rdt+1)&(NRXDESC-1);
    if (!(rx_desc_table[i].status&E1000_RXD_STAT_DD )||!(rx_desc_table[i].status&E1000_RXD_STAT_EOP)) {
        return -1;
    }
    struct rx_desc *head=&rx_desc_table[i];
    uint64_t temp=rd->addr;
    *rd=*head;
    head->addr=temp;
    head->status=0;
    *rdt=i;

    return 0;
}

int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr)
{
  return pci_e1000_attach(pcif);
}

void e1000_send(void *e1000, uint8_t* pkt, uint16_t length)
{
  struct tx_desc td;
  td.addr=(uint64_t)V2P((uint32_t)pkt);
  td.length=length;
  td.cmd=9;
  e1000_put_tx_desc(&td);
}

void e1000_recv(void *e1000, uint8_t* pkt, uint16_t *length)
{
  struct rx_desc rd;
  if(e1000_get_rx_desc(&rd)==0)
  {
    *length=rd.length;
    memmove(pkt,(uint8_t*)P2V((uint32_t*)(uint32_t)rd.addr),*length);
  }
  else
  {
    *length=0;
  }
}