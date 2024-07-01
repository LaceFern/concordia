#!/bin/bash

env "PATH=$PATH" "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" bf_switchd --conf-file ./ccDSM.conf --install-dir $SDE_INSTALL/
