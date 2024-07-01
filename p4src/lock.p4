#define LOCK   (-1)
#define UNLOCK 0

action lock_fail_act() {
    loop_back();
    modify_field(message.mtype, M_LOCK_FAIL);
}

table lock_fail_tbl {
    actions {
        lock_fail_act;
    }
    default_action: lock_fail_act;
}

action after_unlock_act() {
    loop_back();
    modify_field(message.mtype, UNLOCK_ACK);
   // drop();
}

action drop_act() {
    drop();
}

action after_check_fail_unlock_act() {
    loop_back();
    modify_field(message.mtype, M_CHECK_FAIL);
}

// @pragma stage 10
table after_unlock_tbl {
    reads {
        message.mtype: exact;
    }
    actions {
        after_unlock_act;
        drop_act;
    }   
}

@pragma stage 10
table after_check_fail_unlock_tbl {
    actions {
        after_check_fail_unlock_act;
    }
    default_action: after_check_fail_unlock_act;
}

field_list resubmit_data {
    cc_md.dir_table_pos;
    resubmit_md.state;
    resubmit_md.bitmap;
}

action resubmit_act() {
    modify_field(resubmit_md.state, message.state);
    modify_field(resubmit_md.bitmap, message.bitmap);
    resubmit(resubmit_data);
}

table resubmit_tbl {
    actions {
        resubmit_act;
    }
    default_action: resubmit_act;
}

////////////////////////////////////////// 


#define LOCK_REGISTER(i) \
register lock_##i { \
    width : 16; \
    instance_count : SIZE_PER_STAGE; \
}

#define LOCK_LOCK_ALU(i) \
blackbox stateful_alu lock_alu_##i { \
    reg: lock_##i; \
    \
    condition_lo : register_lo == UNLOCK; \
                                          \
    update_lo_1_predicate: condition_lo;  \
    update_lo_1_value : LOCK; \
                               \
    output_value: register_lo; \
    output_dst: check_md.lockSucc; \
    \
    initial_register_lo_value: UNLOCK; \
}

#define LOCK_UNLOCK_ALU(i) \
blackbox stateful_alu unlock_alu_##i { \
    reg: lock_##i; \
 \
    update_lo_1_value: UNLOCK; \
 \
    initial_register_lo_value: UNLOCK; \
}

#define LOCK_LOCK_ACT(i) \
action lock_tbl_act_##i() { \
    lock_alu_##i.execute_stateful_alu(message.index); \
}

#define LOCK_LOCK_TBL(i) \
table lock_tbl_##i { \
    actions { \
        lock_tbl_act_##i; \
    } \
    default_action: lock_tbl_act_##i; \
}

#define LOCK_UNLOCK_ACT(i) \
action unlock_tbl_act_##i() { \
    /* FIXME set loop back */ \
    unlock_alu_##i.execute_stateful_alu(message.index); \
}

#define LOCK_UNLOCK_TBL(i) \
table unlock_tbl_##i { \
    actions { \
        unlock_tbl_act_##i; \
    } \
    default_action:   unlock_tbl_act_##i; \
}

#define LOCK_UNLOCK_REQ_TBL(i) \
table unlock_tbl_req_##i { \
    actions { \
        unlock_tbl_act_##i; \
    } \
    default_action:  unlock_tbl_act_##i; \
}

////////////////////////////////////////////
LOCK_REGISTER(1)

LOCK_LOCK_ALU(1)
LOCK_LOCK_ACT(1)

@pragma stage 1
LOCK_LOCK_TBL(1)

LOCK_UNLOCK_ALU(1)
LOCK_UNLOCK_ACT(1)

@pragma stage 1
LOCK_UNLOCK_TBL(1)

@pragma stage 1
LOCK_UNLOCK_REQ_TBL(1)
////////////////////////////////////////////
LOCK_REGISTER(3)

LOCK_LOCK_ALU(3)
LOCK_LOCK_ACT(3)

@pragma stage 3
LOCK_LOCK_TBL(3)

LOCK_UNLOCK_ALU(3)
LOCK_UNLOCK_ACT(3)

@pragma stage 3
LOCK_UNLOCK_TBL(3)

@pragma stage 3
LOCK_UNLOCK_REQ_TBL(3)
////////////////////////////////////////////
LOCK_REGISTER(5)

LOCK_LOCK_ALU(5)
LOCK_LOCK_ACT(5)

@pragma stage 5
LOCK_LOCK_TBL(5)

LOCK_UNLOCK_ALU(5)
LOCK_UNLOCK_ACT(5)

@pragma stage 5
LOCK_UNLOCK_TBL(5)

@pragma stage 5
LOCK_UNLOCK_REQ_TBL(5)
////////////////////////////////////////////
LOCK_REGISTER(7)

LOCK_LOCK_ALU(7)
LOCK_LOCK_ACT(7)

@pragma stage 7
LOCK_LOCK_TBL(7)

LOCK_UNLOCK_ALU(7)
LOCK_UNLOCK_ACT(7)

@pragma stage 7
LOCK_UNLOCK_TBL(7)

@pragma stage 7
LOCK_UNLOCK_REQ_TBL(7)
////////////////////////////////////////////
LOCK_REGISTER(9)

LOCK_LOCK_ALU(9)
LOCK_LOCK_ACT(9)

@pragma stage 9
LOCK_LOCK_TBL(9)

LOCK_UNLOCK_ALU(9)
LOCK_UNLOCK_ACT(9)

@pragma stage 9
LOCK_UNLOCK_TBL(9)

@pragma stage 9
LOCK_UNLOCK_REQ_TBL(9)
////////////////////////////////////////////
