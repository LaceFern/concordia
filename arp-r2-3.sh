#!/bin/bash

arp -s 10.0.0.1 b8:59:9f:e9:6b:1c -i enp28s0np0
arp -s 10.0.0.2 98:03:9b:ca:48:38 -i enp28s0np0
arp -s 10.0.0.3 98:03:9b:c7:c8:18 -i enp28s0np0
arp -s 10.0.0.4 98:03:9b:c7:c0:a8 -i enp28s0np0

ifconfig enp28s0np0 mtu 4200