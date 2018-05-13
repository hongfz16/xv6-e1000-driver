#include "types.h"
#include "user.h"

int main(void) {
  int MAC_SIZE = 18;
  char* ip = "10.0.0.1";
  char* mac = malloc(MAC_SIZE);
  if(arp("mynet0", ip, mac, MAC_SIZE) < 0) {
    printf(1, "ARP for IP:%s Failed.\n", ip);
  }
  exit();
}
