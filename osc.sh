#!/bin/bash -e
d=$(dirname $(readlink -f $0))

$d/visionp/bin/mtou -I eth0:1 -i 239.255.0.223 -p 10223 -o 127.0.0.1 -P 8005 --reverse &
$d/visionp/bin/visionp 12002 127.0.0.1 8005