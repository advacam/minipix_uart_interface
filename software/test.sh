#!/bin/bash


DO_COMPILATION=true
DO_CLEAN=false

BUILD_DIR="build"

# clean first
if [ -d "$BUILD_DIR" ] && [ "$DO_CLEAN" = "true" ]; then
	rm -r $BUILD_DIR
fi


#  compilation
if [[ "$DO_COMPILATION" = "true" ]]; then
	./compile.sh
fi

#  run test
./example_interface/linux_test/run_tot_toa.sh

#  run analysis
if command -v python &>/dev/null; then
	python ./example_interface/linux_test/analyse.py
else
  	echo "Python is not installed"

	if command -v python3 &>/dev/null; then
		python3 ./example_interface/linux_test/analyse.py
	else
	  	echo "Python 3 is not installed"
	fi  
fi


