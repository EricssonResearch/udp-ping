#!/bin/bash

if ! command -v gnuplot &> /dev/null
then
    echo "GNU Plot not found. On Debian/Ubuntu install using \"sudo apt-get install gnuplot\""
    exit 1
fi

if ! command -v unbuffer &> /dev/null
then
    echo "unbuffer command not found. On Debian/Ubuntu install using \"sudo apt-get install expect\""
    exit 1
fi

n=100

../udpServer > /dev/null&
pid=$!
sleep 0.1
unbuffer ../udpClient -b -i 200 -a 127.0.0.1 -n $n -q 7 -l > plot.txt&
pidClient=$!
sleep 1
gnuplot plot.gp&
pidGP=$!

i=`cat plot.txt | wc -l`
while [ $(($i < $n)) == 1 ]; do
   i=`cat plot.txt | wc -l`
   sleep 0.5
done

kill $pidGP
kill $pidClient
kill $pid

rm plot.txt
