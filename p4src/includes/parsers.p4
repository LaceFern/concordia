parser start {
    return parse_ethernet;
}

#define ETHER_TYPE_IPV4 0x0800
#define ETHER_TYPE_ARP 0x0806
#define ETHER_TYPE_VLAN 0x8100
parser parse_ethernet {
    extract (ethernet);
    return select (latest.etherType) {
        ETHER_TYPE_IPV4: parse_ipv4;
        ETHER_TYPE_ARP: parse_arp;
        ETHER_TYPE_VLAN: parse_vlan;
        default: ingress;
    }
}

parser parse_vlan {
    extract(vlan);
    return select(latest.ethertype) {
        ETHER_TYPE_IPV4: parse_ipv4;
        default: ingress;
    }
}

parser parse_arp {
    extract(arp);
    return ingress;
}

#define IPV4_PROTOCOL_TCP 6
#define IPV4_PROTOCOL_UDP 17
parser parse_ipv4 {
    extract(ipv4);
    return select (latest.protocol) {
        IPV4_PROTOCOL_TCP: parse_tcp;
        IPV4_PROTOCOL_UDP: parse_udp;
        default: ingress;
    }
}

parser parse_tcp {
    extract (tcp);
    return ingress;
}

#define ROCE_V2_D_PORT 4791
#define MESSAGE_PORT 8888
parser parse_udp {
    extract (udp);
    return select (latest.dstPort) {
       /* ROCE_V2_D_PORT: parse_roce; */
        MESSAGE_PORT:   parse_message;
        default: ingress;
    }
}

parser parse_roce {
    extract (bth);
    return ingress;
}

parser parse_message {
    extract(message);
    return ingress;
}
