#!/bin/bash
if [ "help" = "$1" -o ! -n "$1" ]; then
    echo 'nginx test for vkernel'
    echo 'nginx.sh runtime(original/vkernel-runtime/runsc/kata-runtime)'
    echo 'eg: nginx.sh vkernel-runtime'
    exit 0
fi 
if [ "$1" = "original" ]; then
	bash -c "docker run --rm --name mynginx -d -p 8082:80 nginx"
else
	bash -c "docker run --rm --name mynginx --runtime $1 -d -p 8082:80 nginx"
fi
bash -c "docker run --rm jordi/ab -k -c 100 -n 100000 http://172.17.0.1:8082/"
bash -c "docker stop mynginx"
