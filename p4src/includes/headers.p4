header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}
header ethernet_t ethernet;

header_type vlan_t {
    fields {
        pcp             : 3;
        cfi             : 1;
        vid             : 12;
        ethertype       : 16;
    }
}
header vlan_t vlan;

header_type ipv4_t {
    fields {
        version : 4;
        ihl : 4;
        diffserv : 8;
        totalLen : 16;
        identification : 16;
        flags : 3;
        fragOffset : 13;
        ttl : 8;
        protocol : 8;
        hdrChecksum : 16;
        srcAddr : 32;
        dstAddr: 32;
    }
}
header ipv4_t ipv4;

header_type arp_t {
    fields {
        hwType : 16;
        protoType : 16;
        hwAddrLen : 8;
        protoAddrLen : 8;
        opcode : 16;
        hwSrcAddr : 48;
        protoSrcAddr : 32;
        hwDstAddr : 48;
        protoDstAddr : 32;
    }
}

header arp_t arp;


header_type tcp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        seqNo : 32;
        ackNo : 32;
        dataOffset : 4;
        res : 3;
        ecn : 3;
        ctrl : 6;
        window : 16;
        checksum : 16;
        urgentPtr : 16;
    }   
}
header tcp_t tcp;

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        len : 16;
        checksum : 16;
    }
}
header udp_t udp;

header_type bth_t {
    fields {
        ib_op: 8;
        ib_other: 88;
        deth: 64;
        v: 8;
        vv:24;
        icrc: 32;
    }
}
header bth_t bth;

header_type message_t {
    fields {
        qpn: 16;

        mtype: 8;

        dirKey: 32;

        dirNodeID: NODE_ID_WIDTH;
        nodeID: NODE_ID_WIDTH;

        appID: 8;
        mybitmap: BITMAP_WIDTH;

        state:  8;
        bitmap: BITMAP_WIDTH;

        is_app_req: 8;

        index: 32;
        tag  : 32;
    }
}

header message_t message;
