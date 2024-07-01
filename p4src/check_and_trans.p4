
// (10100 & 00010) = 0x0
action check_cc_miss() {
    modify_field(check_md.stateSucc, CHECK_SUCC);
    bit_and(check_md.bitmapSucc, message.bitmap,  message.mybitmap);
}

// (1100 ^ 1100) = 0x0
// ~(10100 | 11011) = 0x0
action check_other(wantState) {
    bit_xor(check_md.stateSucc, message.state, wantState);
    bit_nor(check_md.bitmapSucc, message.bitmap, cc_md.ver_bitmap);
}

table check_valid_tbl {
    reads {
        message.mtype: exact;
    }
    actions {
        check_cc_miss;
        check_other;
    }
    size: 8;
}

/////////////////////////////////////////////////////


// (read_miss, unshared)
// (read_miss, shared)
// (write_miss, unshared)
// (write_evict, dirty)
action trans_nop() {

}

// (read_miss, dirty)
// (write_miss, dirty)
action trans_mcast() {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, 0x1FF);
    modify_field(ig_intr_md_for_tm.mcast_grp_a, message.bitmap);
}

// (write_miss, shared)
action trans_mcast_ucast() {
    modify_field(ig_intr_md_for_tm.mcast_grp_a, message.bitmap);
}

// (write shared)
action trans_mcast_without_myself() {
    bit_and(ig_intr_md_for_tm.mcast_grp_a, message.bitmap, cc_md.ver_bitmap);
    loop_back();
}

// (read_evict, shared)
action trans_loop_back() {
    loop_back();
}

table message_trans_tbl {
    reads {
        message.mtype: exact;
        message.state: exact;
    }
    actions {
        trans_nop;
        trans_mcast;
        trans_mcast_ucast;
        trans_loop_back;
        trans_mcast_without_myself;
    }
    size: 16;
}


action check_fail_act() {
    resubmit(resubmit_data);
}

action check_succ_act() {}
table check_state_tbl {
    reads {
        check_md.bitmapSucc: exact;
        check_md.stateSucc: exact;
    }
    actions {
        check_fail_act;
        check_succ_act;
    }
    default_action: check_fail_act;
    size: 8;
}

control check_and_trans {
    apply(check_valid_tbl);
    if (check_md.lockSucc == LOCK_FAIL) {
        apply(lock_fail_tbl);
    } else {
        apply (check_state_tbl) {
            check_succ_act {
                apply(message_trans_tbl);
            }
        }
    }
}