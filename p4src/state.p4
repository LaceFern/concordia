#define UNSHARED 0
#define SHARED 1
#define DIRTY 2

#define STATE_REGISTER(i) \
register state_##i { \
    width : 8; \
    instance_count : SIZE_PER_STAGE; \
}

#define STATE_READ_ALU(i)\
blackbox stateful_alu read_state_alu_##i { \
    reg : state_##i; \
    \
    output_value: register_lo; \
    output_dst: message.state; \
    \
    initial_register_lo_value: UNSHARED; \
}

#define STATE_WRITE_ALU(i) \
blackbox stateful_alu write_state_alu_##i {\
    reg: state_##i; \
    \
    update_lo_1_value: message.state; \
    \
    initial_register_lo_value: UNSHARED; \
}

#define STATE_READ_ACT(i) \
action read_state_act_##i() { \
    read_state_alu_##i.execute_stateful_alu(message.index); \
}

#define STATE_READ_TBL(i) \
table read_state_tbl_##i { \
    actions { \
        read_state_act_##i; \
    } \
    default_action:   read_state_act_##i; \
}

#define STATE_WRITE_ACT(i) \
action write_state_act_##i() { \
    write_state_alu_##i.execute_stateful_alu(message.index); \
}

#define STATE_WRITE_TBL(i) \
table write_state_tbl_##i { \
    actions { \
        write_state_act_##i; \
    } \
    default_action:  write_state_act_##i; \
}

#define STATE_WRITE_TBL_DIR(i) \
table write_state_tbl_dir_##i { \
    actions { \
        write_state_act_##i; \
    } \
    default_action: write_state_act_##i; \
}


//////////////////////////////////////////////////
STATE_REGISTER(1)

STATE_WRITE_ALU(1)
STATE_WRITE_ACT(1)

@pragma stage 1
STATE_WRITE_TBL(1)

@pragma stage 1
STATE_WRITE_TBL_DIR(1)

STATE_READ_ALU(1)
STATE_READ_ACT(1)

@pragma stage 1
STATE_READ_TBL(1)

/////////////////////////////////////////////////
STATE_REGISTER(3)

STATE_WRITE_ALU(3)
STATE_WRITE_ACT(3)

@pragma stage 3
STATE_WRITE_TBL(3)

@pragma stage 3
STATE_WRITE_TBL_DIR(3)

STATE_READ_ALU(3)
STATE_READ_ACT(3)

@pragma stage 3
STATE_READ_TBL(3)

/////////////////////////////////////////////////
STATE_REGISTER(5)

STATE_WRITE_ALU(5)
STATE_WRITE_ACT(5)

@pragma stage 5
STATE_WRITE_TBL(5)

@pragma stage 5
STATE_WRITE_TBL_DIR(5)

STATE_READ_ALU(5)
STATE_READ_ACT(5)

@pragma stage 5
STATE_READ_TBL(5)

/////////////////////////////////////////////////
STATE_REGISTER(7)

STATE_WRITE_ALU(7)
STATE_WRITE_ACT(7)

@pragma stage 7
STATE_WRITE_TBL(7)

@pragma stage 7
STATE_WRITE_TBL_DIR(7)

STATE_READ_ALU(7)
STATE_READ_ACT(7)

@pragma stage 7
STATE_READ_TBL(7)

/////////////////////////////////////////////////
STATE_REGISTER(9)

STATE_WRITE_ALU(9)
STATE_WRITE_ACT(9)

@pragma stage 9
STATE_WRITE_TBL(9)

@pragma stage 9
STATE_WRITE_TBL_DIR(9)

STATE_READ_ALU(9)
STATE_READ_ACT(9)

@pragma stage 9
STATE_READ_TBL(9)

/////////////////////////////////////////////////
