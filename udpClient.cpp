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
#include "udp-ping.hpp"

#define FIXED 0
#define RANDOM_EXP 1
#define RANDOM_FIXED_PLUS_EXP 2

namespace po = boost::program_options;

int openSocket()
{
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){
        perror("Cannot open socket");
    }
    return fd;
}

bool setToS(int fdSnd, int ptos)
{
    int tos = ptos << 2;
    if(setsockopt(fdSnd, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
        return false;

    return true;
}

bool resetSockets(int &fdSnd, int &fdRcv, int tos, bool thr)
{
    close(fdSnd);
    close(fdRcv);

    if(thr)
    {
        fdSnd = openSocket();
        fdRcv = openSocket();
    }
    else
    {
        fdSnd = fdRcv = openSocket();
    }

    if(fdSnd < 0 || fdRcv <0)
        return false;

    if(!setToS(fdSnd, tos))
        return false;

    return true;
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

long int timespecDiff(const struct timespec ts1, const struct timespec ts2)
{
    return ((ts1.tv_nsec + ts1.tv_sec * 1E9)  - (ts2.tv_nsec + ts2.tv_sec * 1E9));
}

int udpReceive(int fd, std::map<SEQNR_TYPE, timespec> &in, std::map<SEQNR_TYPE, timespec> &oneWay, char* buffer)
{
    fd_set readfds;
    fcntl(fd, F_SETFL, O_NONBLOCK);


    struct timespec now, then;
    struct timeval tv;
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
        memcpy(&then, &buffer[sizeof(SEQNR_TYPE)], sizeof(struct timespec));
        in[seqnr] = now;
        oneWay[seqnr] = then;

        return seqnr;
    }
    return -1;
}


void printResult(std::map<SEQNR_TYPE, timespec> out, std::map<SEQNR_TYPE, timespec> in, std::map<SEQNR_TYPE, timespec> oneWay)
{
    bool warn = false;

    SEQNR_TYPE lost = 0, cnt = 0;
    std::cout << "SeqNr \t SendTime \t ServerTime \t ReceiveTime \t Client->Server \t Server->Client \t RTT (all times in ns)\n";

    for(std::map<SEQNR_TYPE, timespec>::iterator it = out.begin(); it != out.end(); it++)
    {
        cnt++;
        if(in.find(it->first) != in.end())
        {
            std::cout << std::fixed << std::setprecision(0) << it->first << ";"
                << it->second.tv_nsec + it->second.tv_sec * 1E9 << ";"
                << (oneWay[it->first].tv_nsec + oneWay[it->first].tv_sec * 1E9) << ";"
                << in[it->first].tv_nsec + in[it->first].tv_sec * 1E9 << ";"
                << timespecDiff(oneWay[it->first], it->second) << ";"
                << timespecDiff(in[it->first], oneWay[it->first]) << ";"
                << timespecDiff(in[it->first], it->second) << "\n";
            if(timespecDiff(oneWay[it->first], it->second) < 0)
                warn = true;
        }
        else
        {
            lost++;
            std::cout << std::fixed << std::setprecision(0) << it->first << "\t"
                << it->second.tv_nsec + it->second.tv_sec * 1E9 << " lost\n";
        }
    }
    std::cout << lost << " out of " << cnt << " lost\n";

    if(warn)
        std::cout << "\nWarning: Negative one-way-delays Client->Server were encountered. This can happen with very fast links,\n" 
                  << "as the client records time stamp after sending the packet while the server records before sending it.\n"
                  << "Use -b option to force client time stamping before sending (not recommended)\n"; 
}

paramsType parseParams(int argc, char *argv[])
{
    paramsType par;

    try
    {
        std::stringstream s;
        s << "Destination port number (default " << PORT << ")";

        po::options_description desc("Allowed options");
        desc.add_options()
        ("help,h", "Display this help screen")
        ("address,a", po::value<std::string>(&par.host_name), "Destination host IP address")
        ("port,p", po::value<int>(&par.port)->default_value(PORT), s.str().c_str())
        ("num-packets,n", po::value<int>(&par.num_packets)->default_value(5000), "Number of UDP packets to transmit (default 5000)")
        ("packet-size,s", po::value<int>(&par.packet_size)->default_value(50), "Size, in Byte, of each UDP packet (default 50 Byte, conains random ASCII characters)")
        ("interval,i", po::value<float>(&par.interval)->default_value(20.0), 
        	"Interval, in milliseconds, between sending packets (default 20.0 milliseconds, mean value if randomness is used)")
        ("mode,m", po::value<int>(&par.mode)->default_value(0), 
        	"Distribution of interval between packets: 0: Constant, 1: Exponetially distributed, 2: Constant with ratio r," 
        	"adding exponentially distributed random component with ratio 1 - r")
        ("ratio,r", po::value<float>(&par.ratio)->default_value(0.9), "Ratio of constant component for Mode 2 (default 0.9)")
        ("before,b", po::bool_switch(&par.timestamp)->default_value(false), "Use time stamp just before sending the packet, not after (not recommended)")
        ("throughput,t", po::bool_switch(&par.throughput)->default_value(false), "Print additional information supporting throughput measurements."
                "Be sure to also set this parameter on the udpServer. Delay results will not be shown as the server is not sending replies in throughput mode.")
        ("tos,q", po::value<int>(&par.tos)->default_value(0), "Value of IP ToS/DSCP header field (default 0)\nOnly used by sender (udpClient), not copied into receiver (udpServer) reply")
        ("live,l", po::bool_switch(&par.live)->default_value(false), "Display daley information for each incoming packet, not just at the end")
	("change-source-port,c", po::value<int>(&par.changePort)->default_value(0), 
		"Change source port every x packets, where x is the provided parameter. Use large send interval to assure replies are received before the port is changed");

        po::positional_options_description p;
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << "Usage: udpClient -a ServerIP [options]\n";
            std::cout << desc;
            exit(0);
        }

        if (!vm.count("address"))
        {
            std::cout << "Host IP address is required!\n";
            exit(-1);
        }

        if(par.mode == FIXED)
            std::cout << "Sending interval between packets: Constant\n";
        else if(par.mode == RANDOM_EXP)
            std::cout << "Sending interval between packets: Exponetially distributed\nWARNING:" 
                      << "This may result in small spaces between some packets, causing self-queueing "
                      << "that could be falsely interpreted as network delay. Consider using Mode 2 instead.\n";
        else if(par.mode == RANDOM_FIXED_PLUS_EXP)
        {
            if( (par.ratio < 0.0) || (par.ratio > 1.0) )
            {
                std::cout << "Ratio must be between 0 and 1!\n";
                exit(-1);
            }
            else
                std::cout << "Sending interval between packets: Constant with ratio " << par.ratio 
                          << ", adding exponentially distributed random component with ratio " << 1 - par.ratio << "\n";
        }
        else
        {
            std::cout << "Mode must be '0', '1', or '2':  0: Constant, 1: Exponetially distributed, 2:" 
                      << "Constant with ratio r, adding exponentially distributed random component with ratio 1 - r\n";
            exit(-1);
        }
        if(par.packet_size < (sizeof(SEQNR_TYPE) + sizeof(struct timespec)))
        {
            std::cout << "Minimum packet size is " << sizeof(SEQNR_TYPE) + sizeof(struct timespec) << " to fit sequence number and time stamp\n";
            exit(-1);
        }

        if(par.timestamp)
            std::cout << "Sender takes timestamp before sending the packet (not recommeded)\n";
        std::cout << "Destination host IP address: " << par.host_name << "\n";
        std::cout << "Destination port number: " << par.port << "\n";
        std::cout << "Number of UDP packets to transmit: " << par.num_packets << "\n";
        std::cout << "Size of each UDP packet: " << par.packet_size << " Byte\n";
        std::cout << "Value of ToS/DSCP field in IP header: " << par.tos << "\n";
        std::string strInt = (par.mode != FIXED)?"Mean interval":"Interval";
        std::cout << strInt << " between sending packets: " << par.interval << " ms\n\n";

        return par;
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << "\n";
        exit(-1);
    }

}

int main(int argc, char *argv[])
{
    paramsType p;

    SEQNR_TYPE packetID = 0;
    long startTime;
    std::map<SEQNR_TYPE, timespec> sendMap, receiveMap, oneWayMap;

    p = parseParams(argc, argv);

    struct timespec start, now, tim, send;
    tim.tv_sec = 0;
    tim.tv_nsec = p.interval * 1E6;
    float rate = 1.0;

    std::default_random_engine generator;
    if(p.mode == RANDOM_EXP)
        rate = 1.0/float(p.interval);
    if(p.mode == RANDOM_FIXED_PLUS_EXP)
        rate = 1.0/(float(p.interval) * (1.0 - p.ratio));

    std::exponential_distribution<double> distribution(rate);

    sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(p.host_name.c_str());
    servaddr.sin_port = htons(p.port);

    char *msg = (char*)malloc(sizeof(char) * (p.packet_size + 1));
    for(int i = 0; i < p.packet_size; i++)
        msg[i] = rand() % 57 + 65; // Printable ASCII chars for easier debug

    std::cout << "Opening socket\n";
    int fdRcv = openSocket();
    int fdSnd;
    if(p.throughput) // Throughput measurements need sending to block but not delay measurements
        fdSnd = openSocket();
    else
        fdSnd = fdRcv;

    if(fdRcv < 0 || fdSnd < 0)
    {
        std::cout << "Error opening socket for sending or receiving.";
        return -1;
    }

    if(!setToS(fdSnd, p.tos))
    {
        std::cout << "Error setting ToS/DSCP IP header value for socket.";
        return -1;
    }

    std::cout << "Sockets are open with sender ToS/DSCP set to " << p.tos << "\n";

    if(p.throughput)
    {
        std::cout << std::fixed << std::fixed << std::setprecision(3) << "Starting to send at " 
                  << 1E-3 / p.interval * p.packet_size * 8 << " Mbit/s\n";
        std::cout << "Estimated experiment duration: " 
                  << float(p.num_packets) / (1000.0 / p.interval) << " seconds\n";

        clock_gettime(CLOCK_REALTIME, &start);
        startTime = start.tv_nsec + 1E9 * start.tv_sec;

    }
    else
        std::cout << "Starting to send\n";

    for(SEQNR_TYPE i = 0; i < p.num_packets; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        memcpy(msg, &i, sizeof(SEQNR_TYPE)); // Copy the sequence number to the beginning of the message
        udpSend(fdSnd, servaddr, msg, p.packet_size);
        clock_gettime(CLOCK_REALTIME, &send);

        if(p.timestamp)
            sendMap[i] = start;
        else
            sendMap[i] = send;

        if(p.mode == RANDOM_EXP)
            tim.tv_nsec = distribution(generator) * 1E6;
        if(p.mode == RANDOM_FIXED_PLUS_EXP)
            tim.tv_nsec = distribution(generator) * 1E6 + (1E6 * float(p.interval) * p.ratio);

        do
        {
            int seq = udpReceive(fdRcv, receiveMap, oneWayMap, msg);
            if(p.live && seq >= 0)
            {
                std::cout << std::fixed << std::setprecision(0) << seq << ";"
                    << timespecDiff(oneWayMap[seq], sendMap[seq]) << ";"
                    << timespecDiff(receiveMap[seq], oneWayMap[seq]) << ";"
                    << timespecDiff(receiveMap[seq], sendMap[seq]) << "\n";
            }

            clock_gettime(CLOCK_REALTIME, &now);
        }while((now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) < (tim.tv_nsec + tim.tv_sec * 1E9));

	if(p.changePort && (i % p.changePort == 0))
        {
            if(!resetSockets(fdSnd, fdRcv, p.tos, p.throughput))
            {
                std::cout << "Resetting socket failed." << "\n";
                return -1;
            }
        }
    }

    if(!p.throughput) // Keep receiving for 2 more seconds
    {
        tim.tv_sec = 2;
        tim.tv_nsec = 0;
        clock_gettime(CLOCK_REALTIME, &start);
        do
        {
            udpReceive(fdRcv, receiveMap, oneWayMap, msg);
            clock_gettime(CLOCK_REALTIME, &now);
        }while((now.tv_nsec + now.tv_sec * 1E9) - (start.tv_nsec + start.tv_sec * 1E9) < (tim.tv_nsec + tim.tv_sec * 1E9));
    }
    else // Print information about actual throughput
    {
        float duration = float(now.tv_nsec + now.tv_sec * 1E9 - startTime) / 1E9;
        long totalBit = long(p.num_packets) * long(p.packet_size) * 8.0;

        std::cout << "Experiment took " << duration << " seconds.\n";
        std::cout <<  std::fixed << std::setprecision(3) 
                  << "Average sending throughput: " << float(totalBit) / 1E6 
                  << " MBit / " << duration << " s = " << float(totalBit) / duration / 1E6 
                  << " Mbit/s. (If lower than what was configured, client was unable to send faster)\n";
        std::cout << "Client finished. Press Ctrl+C on server to print result.\n";
    }

    free(msg);
    close(fdSnd);
    close(fdRcv);

    if(!p.throughput && !p.live)
        printResult(sendMap, receiveMap, oneWayMap);
    return 0;
}

