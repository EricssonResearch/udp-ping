#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <random>
#include <sstream>
#include <boost/program_options.hpp>
#include <map>

#define FIXED 0
#define RANDOM_EXP 1
#define RANDOM_FIXED_PLUS_EXP 2

#define SEQNR_TYPE long int

#define MAXBUF 1600

namespace po = boost::program_options;

int openSocket()
{
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){
        perror("Cannot open socket");
    }
    return fd;
}


bool udpSend(int fd, sockaddr_in serveraddr, const char *msg, int size)
{
    if (sendto(fd, msg, size, 0,
               (sockaddr*)&serveraddr, sizeof(serveraddr)) < 0){
        perror("Cannot send message");
        return false;
    }
    return true;
}

bool udpReceive(int fd, std::map<SEQNR_TYPE, timespec> &in, char* buffer)
{
    fd_set readfds;
    fcntl(fd, F_SETFL, O_NONBLOCK);


    struct timeval tv;
    struct timespec now;
    SEQNR_TYPE seqnr;
    socklen_t len;
    struct sockaddr_in servaddr;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int rv = select(fd + 1, &readfds, NULL, NULL, &tv);

    if(rv == 1)
    {
        clock_gettime(CLOCK_REALTIME, &now);
        int n = recvfrom(fd, (char*)buffer, MAXBUF, 0, (struct sockaddr *) &servaddr, &len);
        memcpy(&seqnr, buffer, sizeof(SEQNR_TYPE));
        in[seqnr] = now;

        return true;
    }
    return false;
}


void printResult(std::map<SEQNR_TYPE, timespec> out, std::map<SEQNR_TYPE, timespec> in)
{
    SEQNR_TYPE lost = 0, cnt = 0;
    std::cout << "SeqNr \t SendTime \t ReceiveTime \t RTT (all times in ns)\n";

    for(std::map<SEQNR_TYPE, timespec>::iterator it = out.begin(); it != out.end(); it++)
    {
        cnt++;
        if(in.find(it->first) != in.end())
        {
            std::cout << std::fixed << std::setprecision(0) << it->first << ";"
                << it->second.tv_nsec + it->second.tv_sec * 1E9 << ";"
                << in[it->first].tv_nsec + in[it->first].tv_sec * 1E9 << ";"
                << (in[it->first].tv_nsec + in[it->first].tv_sec * 1E9) - (it->second.tv_nsec + it->second.tv_sec * 1E9) << "\n";
        }
        else
        {
            lost++;
            std::cout << std::fixed << std::setprecision(0) << it->first << "\t"
                << it->second.tv_nsec + it->second.tv_sec * 1E9 << " lost\n";
        }
    }
    std::cout << lost << " out of " << cnt << " lost\n";
}

int main(int argc, char *argv[])
{
    const char * host_name;
    std::string tmp_host_name;
    int port, num_packets, packet_size, interval, mode;
    float ratio;
    SEQNR_TYPE packetID = 0;
    std::map<SEQNR_TYPE, timespec> sendMap, receiveMap;

    try
    {
        po::options_description desc("Allowed options");
        desc.add_options()
        ("help,h", "Display this help screen")
        ("address,a", po::value<std::string>(&tmp_host_name), "Destination host IP address")
        ("port,p", po::value<int>(&port)->default_value(1234), "Destination port number (default 1234)")
        ("num-packets,n", po::value<int>(&num_packets)->default_value(5000), "Number of UDP packets to transmit (default 5000)")
        ("packet-size,s", po::value<int>(&packet_size)->default_value(50), "Size, in Byte, of each UDP packet (default 50 Byte, conains random ASCII characters)")
        ("interval,i", po::value<int>(&interval)->default_value(20), "Interval, in milliseconds, between sending packets (default 20 milliseconds, mean value if randomness is used)")
        ("mode,m", po::value<int>(&mode)->default_value(0), "Distribution of interval between packets: 0: Constant, 1: Exponetially distributed, 2: Constant with ratio r, adding exponentially distributed random component with ratio 1 - r")
        ("ratio,r", po::value<float>(&ratio)->default_value(0.9), "Ratio of constant component for Mode 2 (default 0.9)");

        po::positional_options_description p;
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << "Usage: options_description [options]\n";
            std::cout << desc;
            return 0;
        }

        if (vm.count("address"))
        {
            host_name = tmp_host_name.c_str();
        }
        else
        {
            perror("Host IP address is required!\n");
            return -1;
        }

        if(mode == FIXED)
            std::cout << "Sending interval between packets: Constant\n";
        else if(mode == RANDOM_EXP)
            std::cout << "Sending interval between packets: Exponetially distributed\nWARNING: This may result in small spaces between some packets, causing self-queueing that could be falsely interpreted as network delay. Consider using Mode 2 instead.";
        else if(mode == RANDOM_FIXED_PLUS_EXP)
        {
            if( (ratio < 0.0) || (ratio > 1.0) )
            {
                perror("Ratio must be between 0 and 1!\n");
                return -1;
            }
            else
                std::cout << "Sending interval between packets: Constant with ratio " << ratio << ", adding exponentially distributed random component with ratio " << 1 - ratio << "\n";
        }
        else
        {
            perror("Mode must be '0', '1', or '2':  0: Constant, 1: Exponetially distributed, 2: Constant with ratio r, adding exponentially distributed random component with ratio 1 - r\n");
            return -1;
        }
        if(packet_size < sizeof(SEQNR_TYPE))
        {
            std::cout << "Minimum packet size is " << sizeof(SEQNR_TYPE) << " to fit sequence number\n";
            return -1;
        }

        std::cout << "Destination host IP address: " << host_name << "\n";
        std::cout << "Destination port number: " << port << "\n";
        std::cout << "Number of UDP packets to transmit: " << num_packets << "\n";
        std::cout << "Size of each UDP packet: " << packet_size << " Byte\n";
        std::string strInt = (mode != FIXED)?"Mean interval":"Interval";
        std::cout << strInt << " between sending packets: " << interval << " ms\n\n";
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << "\n";
        return 1;
    }


    struct timespec start, now, tim;
    tim.tv_sec = 0;
    tim.tv_nsec = interval * 1E6;
    float rate = 1.0;

    std::default_random_engine generator;
    if(mode == RANDOM_EXP)
        rate = 1.0/float(interval);
    if(mode == RANDOM_FIXED_PLUS_EXP)
        rate = 1.0/(float(interval) * (1.0 - ratio));

    std::exponential_distribution<double> distribution(rate);

    sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host_name);
    servaddr.sin_port = htons(port);

    char *msg = (char*)malloc(sizeof(char) * (packet_size + 1));
    for(int i = 0; i < packet_size; i++)
        msg[i] = rand() % 57 + 65; // Printable ASCII chars for easier debug

    std::cout << "Opening socket\n";
    int fd = openSocket();
    if(fd < 0)
        return -1;

    std::cout << "Socket is open\n";
    std::cout << "Starting to send\n";
    for(SEQNR_TYPE i = 0; i < num_packets; i++)
    {
        memcpy(msg, &i, sizeof(SEQNR_TYPE)); // Copy the sequence number to the beginning of the message
        udpSend(fd, servaddr, msg, packet_size);
        clock_gettime(CLOCK_REALTIME, &start);
        sendMap[i] = start;

        if(mode == RANDOM_EXP)
            tim.tv_nsec = distribution(generator) * 1E6;
        if(mode == RANDOM_FIXED_PLUS_EXP)
            tim.tv_nsec = distribution(generator) * 1E6 + (1E6 * float(interval) * ratio);

        do
        {
            udpReceive(fd, receiveMap, msg);
            clock_gettime(CLOCK_REALTIME, &now);
        }while((now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) < (tim.tv_nsec + tim.tv_sec * 1E9));
        //std::cout << std::fixed << (now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) << "\n";
    }

    // Keep receiving for 2 more seconds
    tim.tv_sec = 2;
    tim.tv_nsec = 0;
    clock_gettime(CLOCK_REALTIME, &start);
    do
    {
        udpReceive(fd, receiveMap, msg);
        clock_gettime(CLOCK_REALTIME, &now);
    }while((now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) < (tim.tv_nsec + tim.tv_sec * 1E9));

    free(msg);
    close(fd);
    
    printResult(sendMap, receiveMap);
    return 0;
}
