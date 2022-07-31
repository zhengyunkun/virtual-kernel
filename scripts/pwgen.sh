#!/bin/bash
if [ "help" = "$1" -o  ! -n "$1" ]; then
    echo 'pwgen test for vkernel'
    echo 'pwgen.sh runtime(original/vkernel-runtime/runsc/kata-runtime)'
    echo 'eg: pwgen.sh vkernel-runtime'
    exit 0
fi 


function getTimestamp() {
    datetime=`date "+%Y-%m-%d %H:%M:%S"` 
    seconds=`date -d "$datetime" +%s` 
    milliseconds=$((seconds*1000+10#`date "+%N"`/1000000)) 
    echo "${milliseconds}"
}

if [ "$1" = "original" ]; then
	startTime=$(getTimestamp)
	bash -c "docker run --rm backplane/pwgen 1024 100"
	endTime=$(getTimestamp)
else
	startTime=$(getTimestamp)
	bash -c "docker run --rm --runtime $1 backplane/pwgen 1024 100"
	endTime=$(getTimestamp)
fi
echo "Runing time:" $((${endTime}-${startTime})) "ms"

