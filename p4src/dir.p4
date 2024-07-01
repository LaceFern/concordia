#define DIR_REGISTER(i) \
register dir_##i {\
    width : 64; \
    instance_count: SIZE_PER_STAGE; \
}

#define DIR_READ_ALU(i) \
blackbox stateful_alu read_dir_alu_##i { \
    reg : dir_##i; \
    \
    output_value: register_hi; \
    output_dst: cc_md.dir_tag; \
    \
    initial_register_hi_value: 0x0; \
}

#define DIR_ADD_ALU(i) \
blackbox stateful_alu add_dir_alu_##i {   \
    reg: dir_##i;                         \
                                          \
    condition_hi : register_hi == 0x0;    \
                                          \
    update_hi_1_predicate: condition_hi;  \
    update_hi_1_value: message.tag;       \
                                          \
    output_value: register_hi;            \
    output_dst:   message.dirKey;         \
                                          \
    initial_register_hi_value: 0x0;       \
}

#define DIR_CLEAR_ALU(i)                   \
blackbox stateful_alu clear_dir_alu_##i {  \
    reg: dir_##i;                          \
                                           \
    condition_hi: message.mtype == DEL_DIR;\
                                           \
    update_hi_1_predicate: condition_hi;   \
    update_hi_1_value: 0x0;                \
                                           \
    initial_register_hi_value: 0x0;        \
}

#define DIR_READ_ACT(i) \
action read_dir_act_##i() { \
    read_dir_alu_##i.execute_stateful_alu(message.index); \
}

#define DIR_READ_TBL(i) \
table read_dir_tbl_##i { \
    actions { \
        read_dir_act_##i; \
    } \
    default_action : read_dir_act_##i; \
}

#define DIR_ADD_ACT(i) \
action add_dir_act_##i() { \
    add_dir_alu_##i.execute_stateful_alu(message.index); \
}

#define DIR_ADD_TBL(i) \
table add_dir_tbl_##i { \
    actions { \
        add_dir_act_##i; \
    } \
    default_action : add_dir_act_##i; \
}

#define DIR_CLEAR_ACT(i) \
action clear_dir_act_##i() { \
    clear_dir_alu_##i.execute_stateful_alu(message.index); \
}

#define DIR_CLEAR_TBL(i) \
table clear_dir_tbl_##i { \
    actions { \
        clear_dir_act_##i; \
    } \
    default_action :  clear_dir_act_##i; \
}


////////////////////////////////////////////////////////////
DIR_REGISTER(0)

DIR_READ_ALU(0)
DIR_READ_ACT(0)
@pragma stage 0
DIR_READ_TBL(0)

DIR_ADD_ALU(0)
DIR_ADD_ACT(0)
@pragma stage 0
DIR_ADD_TBL(0)

DIR_CLEAR_ALU(0)
DIR_CLEAR_ACT(0)
@pragma stage 0
DIR_CLEAR_TBL(0)

///////////////////////////////////////////////////////////
DIR_REGISTER(2)

DIR_READ_ALU(2)
DIR_READ_ACT(2)
@pragma stage 2
DIR_READ_TBL(2)

DIR_ADD_ALU(2)
DIR_ADD_ACT(2)
@pragma stage 2
DIR_ADD_TBL(2)

DIR_CLEAR_ALU(2)
DIR_CLEAR_ACT(2)
@pragma stage 2
DIR_CLEAR_TBL(2)

///////////////////////////////////////////////////////////
DIR_REGISTER(4)

DIR_READ_ALU(4)
DIR_READ_ACT(4)
@pragma stage 4
DIR_READ_TBL(4)

DIR_ADD_ALU(4)
DIR_ADD_ACT(4)
@pragma stage 4
DIR_ADD_TBL(4)

DIR_CLEAR_ALU(4)
DIR_CLEAR_ACT(4)
@pragma stage 4
DIR_CLEAR_TBL(4)

///////////////////////////////////////////////////////////
DIR_REGISTER(6)

DIR_READ_ALU(6)
DIR_READ_ACT(6)
@pragma stage 6
DIR_READ_TBL(6)

DIR_ADD_ALU(6)
DIR_ADD_ACT(6)
@pragma stage 6
DIR_ADD_TBL(6)

DIR_CLEAR_ALU(6)
DIR_CLEAR_ACT(6)
@pragma stage 6
DIR_CLEAR_TBL(6)

///////////////////////////////////////////////////////////
DIR_REGISTER(8)

DIR_READ_ALU(8)
DIR_READ_ACT(8)
@pragma stage 8
DIR_READ_TBL(8)

DIR_ADD_ALU(8)
DIR_ADD_ACT(8)
@pragma stage 8
DIR_ADD_TBL(8)

DIR_CLEAR_ALU(8)
DIR_CLEAR_ACT(8)
@pragma stage 8
DIR_CLEAR_TBL(8)