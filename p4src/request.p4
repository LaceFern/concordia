#define R_READ_MISS    1
#define R_WRITE_MISS   2
#define R_WRITE_SHARED 3
#define R_EVICT_SHARED 4
#define R_EVICT_DIRTY  5

#define R_UNLOCK_EVICT 7
#define R_UNLOCK  8
#define R_READ_MISS_UNLOCK 9

#define ADD_DIR 12
#define DEL_DIR 13

#define DEL_DIR_FAIL 14
#define DEL_DIR_SUCC 15
#define ADD_DIR_FAIL 16
#define ADD_DIR_SUCC 17

//////////////////////////////////////////////////////////////

#define AGENT_ACK_WRITE_MISS 67
#define AGENT_ACK_WRITE_SHARED 68
#define M_LOCK_FAIL 69
#define M_CHECK_FAIL 70
#define UNLOCK_ACK 73
