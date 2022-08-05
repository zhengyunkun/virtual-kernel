#!/bin/bash

if [ "help" = "$1" -o ! -n "$1" ]; then
    echo '[ futex test for container runtime ]'
    echo 'Usage:'
    echo '===>>> run.sh $RUNTIME(insecure/secure/vkernel/kata...) $THREADS_PER_BUCKET'
    echo '===>>> e.g: run.sh vkernel 10'
    exit 0
fi 

### $1: container runtime
### $2: number of threads wait on a single futex hash bucket

args=""

### $config: CPUs and Memory limitted to a futex stress/test container
config=" --cpus=4 -m 4G "

case $1 in
    "insecure")  
        args=" --security-opt seccomp=unconfined --security-opt apparmor=unconfined $config"
    ;;
    "secure") 
        args=$config
    ;;
    "kata")
        args=" --runtime=kata-runtime $config"
    ;;
    "kata-virtiofs")
        args=" --runtime=kata-qemu-virtiofs $config"
    ;;
    "gvisor-ptrace")  
        args=" --runtime=runsc-ptrace $config"
    ;;
    "gvisor-kvm")  
        args=" --runtime=runsc-kvm $config"
    ;;
    "vkernel") 
        args=" --runtime=vkernel-runtime $config"
    ;;
    *)
        echo "input $1 is invalid!"
        exit
    ;;
esac

cpunums=$(cat /proc/cpuinfo| grep "processor"| wc -l)

echo "==========>>> [ futex hash start ]"
containernum=$(expr $2 \* $cpunums \* 256 \* 2 / 10240)
./run-ali.sh "$args" $containernum
echo "==========>>> [ futex hash running... ]"

echo " "
echo "==========>>> [ futex wake running... ]"
for((i=1;i<=10;i++));
do
	docker run -it --rm $args d202181178/futex-test sh -c " perf bench futex wake"
done
echo "==========>>> [ futex wake finish ]"

echo " "
echo "==========>>> [ futex hash stopping... ]"
docker stop $(docker ps -q)
echo "==========>>> [ futex hash stopped ]"
