header_type cc_md_t {
    fields {
        dir_table_pos: 4;
        ver_bitmap: BITMAP_WIDTH;
        dir_tag: 32;
    }
}

metadata cc_md_t cc_md;


header_type route_md_t {
     fields {
        dir_id: 32;
        agent_id: 32;
    }
}

metadata route_md_t route_md;


header_type resubmit_md_t {
    fields {
        state: 8;
        bitmap: BITMAP_WIDTH;
    }
}

metadata resubmit_md_t resubmit_md;

header_type check_md_t {
    fields {
        lockSucc: 8;
        stateSucc: 8;
        bitmapSucc: BITMAP_WIDTH;
    }
}
metadata check_md_t check_md;

/////////////////////////////////////////////////////////

action init_cc_acts() {
 bit_not(cc_md.ver_bitmap, message.mybitmap);
 modify_field(check_md.bitmapSucc, CHECK_FAIL);
 modify_field(check_md.stateSucc, CHECK_FAIL);
 modify_field(check_md.lockSucc, LOCK_FAIL);
}


table init_cc_tbl {
    actions {
        init_cc_acts;
    }
    default_action: init_cc_acts;
}

////////////////////////////////////////////////////

#define SET_TABLE_POS_ACT(i) \
action set_table_pos_act_##i() { \
    modify_field(cc_md.dir_table_pos, i); \
}

#define SET_TABLE_POS_TBL(i)   \
table set_table_pos_tbl_##i {  \
    actions {                  \
        set_table_pos_act_##i; \
    }                          \
    default_action: set_table_pos_act_##i; \
}
 
SET_TABLE_POS_ACT(1)
SET_TABLE_POS_TBL(1)

SET_TABLE_POS_ACT(3)
SET_TABLE_POS_TBL(3)

SET_TABLE_POS_ACT(5)
SET_TABLE_POS_TBL(5)

SET_TABLE_POS_ACT(7)
SET_TABLE_POS_TBL(7)

SET_TABLE_POS_ACT(9)
SET_TABLE_POS_TBL(9)

////////////////////////////////////////////////////
#define PROCESS_STAGE(i) {\            
        apply(set_table_pos_tbl_##i);  \
        if (message.is_app_req == 1 or message.mtype == DEL_DIR) { \
            apply(lock_tbl_##i); \
            apply(read_state_tbl_##i); \
            apply(read_bitmap_tbl_##i); \
        } \
       else if (message.mtype == R_UNLOCK or message.mtype == R_READ_MISS_UNLOCK or message.mtype == R_UNLOCK_EVICT) { \
            apply(unlock_tbl_req_##i);     \
            apply(write_state_tbl_##i); \
            apply(write_bitmap_tbl_##i); \
        } \
    }

control process_cc {
    apply(init_cc_tbl);

    apply(read_dir_tbl_0);
    if (cc_md.dir_tag == message.tag) PROCESS_STAGE(1)
    else {
        apply(read_dir_tbl_2);
        if (cc_md.dir_tag == message.tag) PROCESS_STAGE(3)
        else {
            apply(read_dir_tbl_4);
            if (cc_md.dir_tag == message.tag) PROCESS_STAGE(5)
            else {
                apply(read_dir_tbl_6);
                if (cc_md.dir_tag == message.tag) PROCESS_STAGE(7)
                else {
                    apply(read_dir_tbl_8);
                    if (cc_md.dir_tag == message.tag) PROCESS_STAGE(9)
                }
            }
        }
    }

    apply(before_route_tbl);

    apply(select_dest) {
        to_app_act {
            apply(app_set_port_tbl);
        }
        to_dir_act {
            apply(dir_set_port_tbl);
        }
        to_agent_act {
            apply(agent_set_port_tbl);
        }
    }

    if (cc_md.dir_tag == message.tag) {

        if (message.is_app_req == 1) {
            check_and_trans();
        } else if (message.mtype == R_UNLOCK or message.mtype == R_READ_MISS_UNLOCK or message.mtype == R_UNLOCK_EVICT) {
            apply(after_unlock_tbl);
        } else if (message.mtype == DEL_DIR) {
            del_dir_control();
        }
    }
}

///////////// Resubmit ///////////////

action init_resubmit_acts() {
 modify_field(message.state, resubmit_md.state);
 modify_field(message.bitmap, resubmit_md.bitmap);
}


table init_resubmit_tbl {
    actions {
        init_resubmit_acts;
    }
}

control process_resubmit_unlock {
    apply(init_resubmit_tbl);
    if (cc_md.dir_table_pos == 1) {
        apply(clear_dir_tbl_0);
        apply(unlock_tbl_1);
    }
    else if (cc_md.dir_table_pos == 3) {
        apply(clear_dir_tbl_2);
        apply(unlock_tbl_3);
    }
    else if (cc_md.dir_table_pos == 5) {
        apply(clear_dir_tbl_4);
        apply(unlock_tbl_5);
    }
    else if (cc_md.dir_table_pos == 7) {
        apply(clear_dir_tbl_6);
        apply(unlock_tbl_7);
    }
    else if (cc_md.dir_table_pos == 9) {
        apply(clear_dir_tbl_8);
        apply(unlock_tbl_9);
    }

    if (message.mtype == DEL_DIR) {
        apply(del_succ_resubmit_tbl);
    } else {
        apply(after_check_fail_unlock_tbl);
    }
}
