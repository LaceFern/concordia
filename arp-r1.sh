#!/bin/bash

arp -s 10.0.0.1 b8:59:9f:e9:6b:1c -i enp65s0np0
arp -s 10.0.0.2 98:03:9b:ca:48:38 -i enp65s0np0
arp -s 10.0.0.3 98:03:9b:c7:c8:18 -i enp65s0np0
arp -s 10.0.0.4 98:03:9b:c7:c0:a8 -i enp65s0np0
arp -s 10.0.0.5 04:3f:72:de:ba:44 -i enp65s0np0
arp -s 10.0.0.6 98:03:9b:c7:c7:fc -i enp65s0np0
arp -s 10.0.0.7 98:03:9b:ca:40:18 -i enp65s0np0
arp -s 10.0.0.8 0c:42:a1:2b:0d:70 -i enp65s0np0

ifconfig enp65s0np0 mtu 4200