#!/bin/bash

# get the path to this script
MY_PATH=`dirname "$0"`
MY_PATH=`( cd "$MY_PATH" && pwd )`

cd $MY_PATH

#./build/example_interface /tmp/ttyS1 921600 1 /tmp/ttyS2 921600 1

# PATH_BUILD="/home/advacam/moonlander/minipix_uart_interface/software/lukas_adjusted/build"

PATH_BUILD="../../build"
DATA_OUT="./out/23_04_18_ida_test/data.clog"

socat -d -d PTY,link=/tmp/ttyS2,rawer,echo=0 PTY,link=/tmp/ttyS3,rawer,echo=0 &
sleep 0.5; $PATH_BUILD/example_interface/linux_test/example_interface_lxtest /dev/minipix 921600 0 /tmp/ttyS2 921600 1 &
sleep 1.0; $PATH_BUILD/gatherer/gatherer_tot_toa /tmp/ttyS3 921600 1 $DATA_OUT 1000 20 1
kill $(jobs -p)

