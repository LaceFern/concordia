#!/bin/bash

export LD_LIBRARY_PATH=$SDE_INSTALL/lib; $SDE_INSTALL/bin/bf_switchd --conf-file ./ccDSM.conf --install-dir $SDE_INSTALL/