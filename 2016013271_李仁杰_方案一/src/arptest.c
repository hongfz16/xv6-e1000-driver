#include "types.h"
#include "user.h"

int main(int argc,char** argv) {
  int MAC_SIZE = 18;
//  char* ip = "104.236.20.60";
  char* ip = "10.0.2.2";
  char* mac = malloc(MAC_SIZE);
  if(arp("mynet0", argv[1], mac, MAC_SIZE) < 0) {
    printf(1, "ARP for IP:%s Failed.\n", ip);
  }
  exit();
}
