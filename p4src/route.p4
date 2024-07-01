
action loop_back() {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);
    modify_field(udp.srcPort, message.qpn);
}

action set_egr(port) {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
    add_to_field(ipv4.ttl, -1);
}

action ethernet_set_mac_act(dmac) {
    modify_field(ethernet.dstAddr, dmac);
}

@pragma stage 11
table ipv4_route {
    reads {
        ipv4.dstAddr : exact;
    }
    actions {
        set_egr;
    }
    size: 1024;
}

@pragma stage 11
table ethernet_set_mac {
    reads {
        eg_intr_md.egress_port: exact;
    }
    actions {
        ethernet_set_mac_act;
    }
    size: 512;
}
