#include "types.h"
#include "defs.h"
#include "arp_frame.h"
#include "nic.h"
#include "e1000.h"


int send_arpRequest(char* interface, char* ipAddr, char* arpResp) {
  cprintf("Create arp request for ip:%s over Interface:%s\n", ipAddr, interface);
  struct nic_device *nd;
  if(get_device(interface, &nd) < 0) {
    cprintf("ERROR:send_arpRequest:Device not loaded\n");
    return -1;
  }

  struct ethr_hdr eth;
  create_eth_arp_frame(nd->mac_addr, ipAddr, &eth);
  nd->send_packet(nd->driver, (uint8_t*)&eth, sizeof(eth)-2); //sizeof(eth)-2 to remove padding. padding was necessary for alignment.
    return 0;
}
