#!/bin/bash

doTC=true

if command -v sudo tc &> /dev/null
then

    if [ "$EUID" -ne 0 ]
    then 
       echo " "
       echo "##################################################"
       echo "Consider running as root/sudo'er to artificially add delay using TrafficControl (tc)."
       echo "In case you get a warning about negative delays at the end, ignore it."
       echo "#################################################"
       echo " "
       doTC=false
    fi
else
    echo " "
    echo "##################################################"
    echo "TrafficControl (tc) command not found. On Debian/Ubuntu install using \"sudo apt-get install iproute2\""
    echo "#################################################"
    echo " "
    doTC=false
fi

if [ "$doTC" = true ]
then
    echo " "
    echo "##################################################"
    echo "TrafficControl (tc): Adding 10 ms artifical delay to loopback (lo) interface, resulting in 20 ms extra round-trip-time."
    echo "#################################################"
    echo " "
   tc qdisc add dev lo root netem delay 10ms
fi

../udpServer &
pid=$!
sleep 0.1
../udpClient -a 127.0.0.1 -n 200
kill $pid

if [ "$doTC" = true ]
then
   tc qdisc del dev lo root netem delay 10ms
fi

