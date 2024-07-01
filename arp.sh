#!/bin/bash

arp -s 192.168.2.111 ec:0d:9a:ae:14:8c -i ens2
arp -s 192.168.2.112 ec:0d:9a:ae:14:98 -i ens2
arp -s 192.168.2.113 ec:0d:9a:ae:14:90 -i ens2
arp -s 192.168.2.114 ec:0d:9a:c0:40:90 -i ens2
arp -s 192.168.2.115 ec:0d:9a:c0:41:c0 -i ens2
arp -s 192.168.2.116 ec:0d:9a:ae:14:80 -i ens2
arp -s 192.168.2.117 ec:0d:9a:c0:41:cc -i ens2
arp -s 192.168.2.118 ec:0d:9a:ae:14:88 -i ens2
arp -s 192.168.2.121 ec:0d:9a:c0:41:dc -i ens2

ifconfig ens2 mtu 4200
