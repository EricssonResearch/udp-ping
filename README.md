To compile on Debian/Ubuntu and similar:  

sudo apt-get install build-essential  
sudo apt-get install libboost-program-options-dev  
Optionally:  
sudo apt-get install cmake  

From command line:  
g++ -o udpServer udpServer.cpp -lboost_program_options  
g++ -o udpClient udpClient.cpp -lboost_program_options  

When using cmake:  
mkdir build  
cd build  
cmake ..  
make


Example 1: Delay measurement with default values (5000 packets, each 50 byte, port 1234, fixed spacing of 20 ms between packets):  
Server: ./udpServer  
Client: ./udpClient -a ServerIP  
The server reports progress every 100 packets, the client presents results at the end  

Example 2: Throughput measurement sending 10 Mbit/s for 20 seconds:  
Server: ./udpServer -t  
Client: ./udpClient -t -a ServerIP -i 1 -n 20000 -s 1250  
Press Ctrl+C at server when client finished to print result  

Help on command-line options:  
./udpServer -h  
./udpClient -h  
 

