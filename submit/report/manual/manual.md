# Manual

## Environment
- Ubuntu 16.04 LTS
- gcc (Ubuntu 5.4.0-6ubuntu1~16.04.10) 5.4.0 20160609
- QEMU emulator version 2.3.0, Copyright (c) 2003-2008 Fabrice Bellard

## Patch(Optional)
- First download [mit-pdos/xv6-public](https://github.com/mit-pdos/xv6-public)
- Copy `./patch/0001-to-patch.patch` to the above folder
- Run `git apply 0001-to-patch.patch`

## Build
- Use command `make qemu`

## Test
- To test arp: `arptest <IP-ADDRESS>`
- To test icmp:
	- `icmptest`
	- Quit qemu
	- Use command `tcpdump -XXnr qemu.pcap`
