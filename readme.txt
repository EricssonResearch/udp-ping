To compile on Debian/Ubuntu and similar:

sudo apt-get install build-essential
sudo apt-get install libboost-program-options-dev

g++ -o udpServer udpServer.cpp 
g++ -o udpClient udpClient.cpp -lboost_program_options

Simple run with default values (5000 packets, each 50 byte, port 1234, fixed spacing of 20 ms between packets):
Server: ./udpServer
Client: ./udpClient -a TargetIP
The server reports progress every 100 packets, the client presents results at the end

Help on command-line options:
./udpServer -h (changing port (-p) is the only option)
./udpClient -h 

