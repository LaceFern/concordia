
#define BITMAP_REGISTER(i)\
register bitmap_##i {\
    width : 32; \
    instance_count : SIZE_PER_STAGE; \
}

#define BITMAP_READ_ALU(i) \
blackbox stateful_alu read_bitmap_alu_##i { \
    reg : bitmap_##i; \
    \
    output_value: register_hi; \
    output_dst: message.bitmap; \
    \
    initial_register_hi_value: 0x0; \
}

/*
  condition_lo : message.mtype == R_READ_MISS_UNLOCK;    \
                                                           \
    update_lo_1_predicate: condition_lo;                   \
    update_lo_1_value: register_lo + message.bitmap;       \
                                                           \
    update_lo_2_predicate: not condition_lo;               \
*/

#define BITMAP_WRITE_ALU(i)                                \
blackbox stateful_alu write_bitmap_alu_##i {               \
    reg: bitmap_##i;                                       \
                                                           \
    condition_hi : message.mtype == R_READ_MISS_UNLOCK;    \
                                                           \
    update_hi_1_predicate: condition_hi;                   \
    update_hi_1_value: register_hi | message.bitmap;       \
                                                           \
    update_hi_2_predicate: not condition_hi;               \
    update_hi_2_value: message.bitmap;                     \
                                                           \
    initial_register_hi_value: 0x0;                        \
}

// #define BITMAP_AND_ALU(i) \
// blackbox stateful_alu and_bitmap_alu_##i { \
//     reg: bitmap_##i; \
//     \
//     update_lo_1_value: register_lo & cc_md.ver_bitmap; \
//     \
//     initial_register_lo_value: 0x0; \
// }

#define BITMAP_READ_ACT(i) \
action read_bitmap_act_##i() { \
    read_bitmap_alu_##i.execute_stateful_alu(message.index); \
}

#define BITMAP_READ_TBL(i) \
table read_bitmap_tbl_##i { \
    actions { \
        read_bitmap_act_##i; \
    } \
    default_action: read_bitmap_act_##i; \
}

#define BITMAP_WRITE_ACT(i) \
action write_bitmap_act_##i() { \
    write_bitmap_alu_##i.execute_stateful_alu(message.index); \
}

#define BITMAP_WRITE_TBL(i) \
table write_bitmap_tbl_##i { \
    actions { \
        write_bitmap_act_##i; \
    } \
    default_action: write_bitmap_act_##i;\
}

#define BITMAP_WRITE_TBL_DIR(i) \
table write_bitmap_tbl_dir_##i { \
    actions { \
        write_bitmap_act_##i; \
    } \
    default_action: write_bitmap_act_##i; \
}

#define BITMAP_AND_ACT(i) \
action and_bitmap_act_##i() { \
    and_bitmap_alu_##i.execute_stateful_alu(message.index); \
} 

#define BITMAP_AND_TBL(i) \
table and_bitmap_tbl_##i { \
    actions { \
        and_bitmap_act_##i; \
    } \
    default_action:  and_bitmap_act_##i; \
}

////////////////////////////////////////////////////////////
BITMAP_REGISTER(1)

BITMAP_READ_ALU(1)
BITMAP_READ_ACT(1)
@pragma stage 1
BITMAP_READ_TBL(1)

BITMAP_WRITE_ALU(1)
BITMAP_WRITE_ACT(1)

@pragma stage 1
BITMAP_WRITE_TBL(1)

@pragma stage 1
BITMAP_WRITE_TBL_DIR(1)

// BITMAP_AND_ALU(1)
// BITMAP_AND_ACT(1)
// @pragma stage 1
// BITMAP_AND_TBL(1)

///////////////////////////////////////////////////////////
BITMAP_REGISTER(3)

BITMAP_READ_ALU(3)
BITMAP_READ_ACT(3)

@pragma stage 3
BITMAP_READ_TBL(3)

BITMAP_WRITE_ALU(3)
BITMAP_WRITE_ACT(3)

@pragma stage 3
BITMAP_WRITE_TBL(3)

@pragma stage 3
BITMAP_WRITE_TBL_DIR(3)

// BITMAP_AND_ALU(3)
// BITMAP_AND_ACT(3)
// @pragma stage 3
// BITMAP_AND_TBL(3)

///////////////////////////////////////////////////////////
BITMAP_REGISTER(5)

BITMAP_READ_ALU(5)
BITMAP_READ_ACT(5)
@pragma stage 5
BITMAP_READ_TBL(5)

BITMAP_WRITE_ALU(5)
BITMAP_WRITE_ACT(5)

@pragma stage 5
BITMAP_WRITE_TBL(5)

@pragma stage 5
BITMAP_WRITE_TBL_DIR(5)

// BITMAP_AND_ALU(5)
// BITMAP_AND_ACT(5)
// @pragma stage 5
// BITMAP_AND_TBL(5)

///////////////////////////////////////////////////////////
BITMAP_REGISTER(7)

BITMAP_READ_ALU(7)
BITMAP_READ_ACT(7)
@pragma stage 7
BITMAP_READ_TBL(7)

BITMAP_WRITE_ALU(7)
BITMAP_WRITE_ACT(7)

@pragma stage 7
BITMAP_WRITE_TBL(7)

@pragma stage 7
BITMAP_WRITE_TBL_DIR(7)

// BITMAP_AND_ALU(7)
// BITMAP_AND_ACT(7)
// @pragma stage 7
// BITMAP_AND_TBL(7)

///////////////////////////////////////////////////////////
BITMAP_REGISTER(9)

BITMAP_READ_ALU(9)
BITMAP_READ_ACT(9)
@pragma stage 9
BITMAP_READ_TBL(9)

BITMAP_WRITE_ALU(9)
BITMAP_WRITE_ACT(9)

@pragma stage 9
BITMAP_WRITE_TBL(9)

@pragma stage 9
BITMAP_WRITE_TBL_DIR(9)


// BITMAP_AND_ALU(9)
// BITMAP_AND_ACT(9)
// @pragma stage 9
// BITMAP_AND_TBL(9)

///////////////////////////////////////////////////////////
