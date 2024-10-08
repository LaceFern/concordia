action before_route_act() {
    // bit_and(route_md.dir_id, message.dirKey, DIR_ID_MASK);
    // bit_and(route_md.agent_id, message.dirKey, AGENT_ID_MASK);
    modify_field_with_shift(route_md.dir_id, message.dirKey, 24, DIR_ID_MASK);
    modify_field_with_shift(route_md.agent_id, message.dirKey, 24, AGENT_ID_MASK);
}

table before_route_tbl {
    actions {
        before_route_act;
    }
    default_action: before_route_act;
}

/////////////////////////////////////////////////////
action set_port_and_qpn(port, qpn) {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
    modify_field(udp.srcPort, qpn);
}

action set_qpn(qpn) {
    modify_field(udp.srcPort, qpn);
}

//////////////////////////////////////////////////////

@pragma stage 10
table dir_set_port_tbl {
    reads {
        message.dirNodeID: exact;
        route_md.dir_id: exact;
    }
    actions {
        set_port_and_qpn;
    }
    size: 512;
}

///////////////////////////////////////////////////

@pragma stage 10
table app_set_port_tbl {
    reads {
        message.nodeID: exact;
        message.appID: exact;
    }
    actions {
        set_port_and_qpn;
    }
    size: 512;
}

//////////////////////////////////////////////////////////

@pragma stage 10
table agent_set_port_tbl {
    reads {
        message.mybitmap: exact;
        route_md.agent_id: exact;
    }
    actions {
        set_port_and_qpn;
    }
    size: 512;
}

@pragma stage 11
table agent_set_tbl {
    reads {
        eg_intr_md.egress_port: exact;
        route_md.agent_id: exact;
    }
    actions {
        set_qpn;
    }
    size: 512;

}
/////////////////////////////////////////////////////

action to_dir_act()   {}

action to_agent_act() {
}

action to_app_act()   {}

@pragma stage 10
table select_dest {
    reads {
        message.mtype : range ;
    }
    actions {
        to_dir_act;
        to_agent_act;
        to_app_act;
    }
    size: 16;
}