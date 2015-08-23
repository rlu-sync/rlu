#!/bin/bash

#echo Set paths: 
#echo export LD_LIBRARY_PATH=/usr/local/lib
#export LD_LIBRARY_PATH=/usr/local/lib

echo Set malloc:

echo export LD_PRELOAD=/home/amatveev/tools/gperftools-2.1/.libs/libtcmalloc_minimal.so
export LD_PRELOAD=/home/amatveev/tools/gperftools-2.1/.libs/libtcmalloc_minimal.so
#echo export GPERFTOOLS_LIB=/home/amatveev/tools/gperftools-2.1/.libs
#export GPERFTOOLS_LIB=/home/amatveev/tools/gperftools-2.1/.libs

#echo export LD_PRELOAD="/localdisk/amatveev/tools/gperftools-2.4/.libs/libtcmalloc_minimal.so"
#export LD_PRELOAD="/localdisk/amatveev/tools/gperftools-2.4/.libs/libtcmalloc_minimal.so"
