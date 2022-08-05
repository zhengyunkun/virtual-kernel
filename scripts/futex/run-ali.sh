#!/bin/bash

### $1: container options (runtime, cpus, mem, etc.)
### $2: number of containers to run

echo "[ Container options: $1 ]"
echo "[ Number of futex hash containers: $2 ]"
for((i=1;i<=$2;i++));
do
    docker run -itd --rm $1 d202181178/futex-test sh -c "perf bench futex hash -t 10240 -f 1"
done

