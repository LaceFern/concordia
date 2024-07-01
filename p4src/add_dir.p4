action add_dir_succ_act() {
    loop_back();
    modify_field(message.mtype, ADD_DIR_SUCC);
}

action add_dir_fail_act() {
    loop_back();
    modify_field(message.mtype, ADD_DIR_FAIL);
}

@pragma stage 10
table add_dir_succ_tbl {
    actions {
        add_dir_succ_act;
    }
}

@pragma stage 10
table add_dir_fail_tbl {
    actions {
        add_dir_fail_act;
    }
}


control process_add_dir {
    apply(add_dir_tbl_0);
    if (message.dirKey == 0) {
        apply(write_state_tbl_dir_1);
        apply(write_bitmap_tbl_dir_1);
    }
    else {
        apply(add_dir_tbl_2);
        if (message.dirKey == 0) {
          apply(write_state_tbl_dir_3);
          apply(write_bitmap_tbl_dir_3);
        }
        else {
            apply(add_dir_tbl_4);
            if (message.dirKey == 0) {
              apply(write_state_tbl_dir_5);
              apply(write_bitmap_tbl_dir_5);
            }
            else {
                apply(add_dir_tbl_6);
                if (message.dirKey == 0) {
                  apply(write_state_tbl_dir_7);
                  apply(write_bitmap_tbl_dir_7);
                }
                else {
                    apply(add_dir_tbl_8);
                    if (message.dirKey == 0) {
                      apply(write_state_tbl_dir_9);
                      apply(write_bitmap_tbl_dir_9);
                    }
                }
            }
        }
    }

    if (message.dirKey == 0) {
      apply(add_dir_succ_tbl);
    } else {
      apply(add_dir_fail_tbl);
    }
    
}
