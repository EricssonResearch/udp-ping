#!/bin/bash

# The MTU size is set and then auto-detected.
# Makes no sense here, but serves as example how tracepath can
# be used to determine path MTU size.
useMTU=1400

doAutoMTU=true

if !command -v tracepath &> /dev/null
then
    echo "tracepath command not found. On Debian/Ubunut install using \"sudo apt-get install iputils-tracepath\""
    doAutoMTU=false
fi

if !command -v awk &> /dev/null
then
    echo "awk command not found. On Debian/Ubuntu install using \"sudo apt-get install gawk\""
    doAutoMTU=false
fi

doTC=true

if command -v sudo tc &> /dev/null
then

    if [ "$EUID" -ne 0 ]
    then 
       echo " "
       echo "##################################################"
       echo "Consider running as root/sudo'er to artificially limit throughput using TrafficControl (tc) and changing MTU size."
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
    oldMTU=`cat /sys/class/net/lo/mtu`

    echo " "
    echo "##################################################"
    echo "TrafficControl (tc): Artifically limiting throughput of loopback (lo) interface to 500 Mbit/s."
    tc qdisc add dev lo root tbf rate 500Mbit burst 2000 latency 100ms

    echo "Changing loopback (lo) interface MTU size from $oldMTU to $useMTU."
    ip link set dev lo mtu $useMTU
fi

if [ "$doAutoMTU" = true ]
then
    mtu=`tracepath 127.0.0.1 | tail -1 | awk '{print $3}'`
    echo "Path MTU size detected: $mtu Byte"
else
    mtu=$(($useMTU-60))
    echo "Cannot automatically detect path MTU size. Using $mtu Byte"
fi

ps=$(($mtu-60))
echo "Using packet size $mtu - 60 = $ps Byte"

echo "#################################################"
echo " "

../udpServer -t&
pid=$!
sleep 0.1
../udpClient -t -a 127.0.0.1 -n 300000 -i 0.01 -s $ps
kill -SIGINT $pid

sleep 0.5

if [ "$doTC" = true ]
then
   tc qdisc del dev lo root tbf rate 500Mbit burst 2000 latency 100ms
   ip link set dev lo mtu $oldMTU

   echo " "
   echo "##################################################"
   echo "Change of MTU size reverted and throughput limitation on loopback (lo) interface removed."
   echo "##################################################"
   echo " "
fi
