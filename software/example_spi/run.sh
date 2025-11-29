#!/bin/bash

DATA_PATH=$PWD/data.txt

# get the path to this script
MY_PATH=`dirname "$0"`
MY_PATH=`( cd "$MY_PATH" && pwd )`

cd $MY_PATH

./_build/example_spi $DATA_PATH 1
 