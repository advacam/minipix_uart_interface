#!/bin/bash

# get the path to this script
MY_PATH=`dirname "$0"`
MY_PATH=`( cd "$MY_PATH" && pwd )`

cd $MY_PATH

PATH_BUILD="../../build"
DATA_OUT_PATH="./out/test"
DATA_OUT_NAME="data.txt"

FRAME_TIME=250	# ms
FRAME_COUNT=10

if [[ ! -d "$DATA_OUT_PATH" ]]; then
	mkdir $DATA_OUT_PATH
fi

socat -d -d PTY,link=/tmp/ttyS2,rawer,echo=0 PTY,link=/tmp/ttyS3,rawer,echo=0 &
sleep 0.5; $PATH_BUILD/example_interface/linux_test/example_interface_lxtest /dev/minipix 921600 0 /tmp/ttyS2 921600 1 &
sleep 1.0; $PATH_BUILD/gatherer/gatherer_tot_toa /tmp/ttyS3 921600 1 $DATA_OUT_PATH/$DATA_OUT_NAME $FRAME_TIME $FRAME_COUNT 1
kill $(jobs -p)

