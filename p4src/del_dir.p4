action del_succ_resubmit_act() {
    loop_back();
    modify_field(message.mtype, DEL_DIR_SUCC);
}

table del_succ_resubmit_tbl {
    actions {
        del_succ_resubmit_act;
    }
    default_action: del_succ_resubmit_act;
}

action del_fail_act() {
    loop_back();
    modify_field(message.mtype, DEL_DIR_FAIL);
}


table del_fail_tbl {
    actions {
        del_fail_act;
    }
}

control del_dir_control {
    if (check_md.lockSucc == LOCK_FAIL) {
        apply(del_fail_tbl);
    } else {
        apply(resubmit_tbl);
    }
}