#include "define.p4"

#include "includes/headers.p4"
#include "includes/parsers.p4"
#include "includes/checksum.p4"

#include <tofino/intrinsic_metadata.p4>
#include <tofino/constants.p4>
#include <tofino/stateful_alu_blackbox.p4>

#include "request.p4"

#include "route.p4"

// #include "rw_lock.p4"
#include "lock.p4"
#include "state.p4"
#include "bitmap.p4"
#include "dir.p4"

#include "add_dir.p4"
#include "del_dir.p4"
#include "check_and_trans.p4"
#include "cc_route.p4"
#include "cc.p4"

control ingress {
    if (valid(message)) {

        if (ig_intr_md.resubmit_flag != 0) {
            process_resubmit_unlock();
        } else if (message.mtype == ADD_DIR) {
            process_add_dir();
        } else {
            process_cc();
        }
    } else if (valid(ipv4)) {
        apply(ipv4_route);
    }
}

control egress {

    apply(ethernet_set_mac);
    if (valid(message)) {
        if (eg_intr_md.egress_rid == 233) {
            apply(agent_set_tbl);
        }
    }
}
