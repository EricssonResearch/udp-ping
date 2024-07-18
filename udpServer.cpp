// Server side implementation of UDP client-server model
#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include <boost/program_options.hpp>
#include "udp-ping.hpp"

namespace po = boost::program_options;

std::map<SEQNR_TYPE, std::pair<timespec, int>> received; // Needs to be global to access in Ctrl+C handler
bool printRaw;

int getToS(struct msghdr msg)
{
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg,cmsg)) 
    {
        if((cmsg->cmsg_level == IPPROTO_IP) && (cmsg->cmsg_type == IP_TOS) && (cmsg->cmsg_len))
        {
            int tos = *(uint8_t *)CMSG_DATA(cmsg);
            return (tos >> 2);
        }
    }
    return -1;
}

void printResult()
{
    std::map<SEQNR_TYPE, std::pair<timespec, int>>::iterator it;
    std::cout << "\n";
    for(it = received.begin(); it != received.end(); it++)
    {
       std::cout << std::fixed << std::setprecision(0) << it->first << "; " << it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec 
                 << "; " << it->second.second << "\n";
    }
}

void printPerSecondResult()
{
    std::map<SEQNR_TYPE, std::pair<timespec, int>>::iterator it;
    std::cout << "\n";
    long lastTime, thisTime, passedTime, timeSum, bitSum, lost, startTime, allBit;
    SEQNR_TYPE lastID, thisID, count;

    // Init with values from first entry
    it = received.begin();
    lastTime = it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec;
    bitSum = it->second.second;
    lastID = it->first;
    timeSum = 0;
    lost = 0;
    count = 1;
    it++;
    startTime = lastTime;
    allBit = 0;

    for( ; it != received.end(); it++)
    {
       thisID = it->first;
       thisTime = it->second.first.tv_sec * 1E9 + it->second.first.tv_nsec;
       passedTime = thisTime - lastTime;
       timeSum += passedTime;
       lastTime = thisTime;

       if(timeSum >= 1E9)
       {
           std::cout << std::fixed << std::setprecision(3) << float(bitSum) / 1E6 << " Mbit/s " << count << " packets\n";
           
           lastTime = thisTime;
           bitSum = it->second.second;
           timeSum -= 1E9;
           count = 1;
       }
       else
       { 
           bitSum += it->second.second;
           count++;
       }
       lost += (thisID - lastID - 1);
       lastID = thisID;
       allBit += it->second.second;
    }
    std::cout << std::fixed << std::setprecision(3) 
              << lost << " of " << received.size() + lost << " (" 
              << 100.0 * float(lost)/float(received.size() + lost)  
              << "\%) packets lost. Experiment took " << float(lastTime - startTime) / 1E9  
              << " seconds.\n"
              << "Average receiving throughput: " << float(allBit) / 1E6 / (float(lastTime - startTime) / 1E9) << " Mbit/s\n";
}

void
inthand(int signum)
{
    if(received.size())
    {
        if(printRaw)
            printResult();
        else
            printPerSecondResult();
    }
    exit(0);
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
        ("port,p", po::value<int>(&par.port)->default_value(PORT), s.str().c_str())
        ("throughput,t", po::bool_switch(&par.throughput)->default_value(false), "Throughput measurement mode (should be also set on client). "
                "No replies will be sent for delay measurement.")
	("raw,r", po::bool_switch(&printRaw)->default_value(false), "Display raw throughput results with size and time stamp of each received packet.");
        
        po::positional_options_description p;
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << "Usage: udpServer [options]\n";
            std::cout << desc;
            exit(0);
        }
        if(printRaw & !par.throughput)
        {
            std::cout << "Please use -t option with -r option\n";
            exit(0);
        }
        
        std::cout << "Port: " << par.port << "\n";
        if(par.throughput)
        {
            std::cout << "Throughput mode. Press Ctrl+C when client done sending.\n"
                      << "Results will be printed " << (printRaw?"raw":"per second") << ".\n";
        }

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
    int packet_size;
    long int count = 0;
    paramsType p;

    p = parseParams(argc, argv);

    if(p.throughput)
        signal(SIGINT, inthand);

    int sockfd;
    char buffer[MAXBUF];
    
    unsigned char received_tos;

    struct msghdr msg;
    struct iovec iov[1];
    memset(&msg, '\0', sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    iov[0].iov_base = &buffer;
    iov[0].iov_len = sizeof(buffer);

    int cmsg_size = sizeof(struct cmsghdr) + sizeof(received_tos);
    char buf[CMSG_SPACE(sizeof(received_tos))];

    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct sockaddr_in servaddr, cliaddr;
    msg.msg_name = &cliaddr;
    msg.msg_namelen = sizeof(cliaddr);

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) 
    {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
    }

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(p.port);

    // Bind the socket with the server address
    if(bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Binding to socket failed");
        return -1;
    }

    // Enabling extracting ToS/DSCP information from received packets
    unsigned char set = 0x03;
    setsockopt(sockfd, IPPROTO_IP, IP_RECVTOS, &set, sizeof(set));

    struct timespec now;
    socklen_t len;
    int n = 0;

    while(1)
    {
        n = recvmsg(sockfd, &msg, 0);
        
        if (n > 0)
        {
            clock_gettime(CLOCK_REALTIME, &now);

            if(!p.throughput)
            {
                memcpy(&buffer[sizeof(SEQNR_TYPE)], &now, sizeof(struct timespec));
                if(sendto(sockfd, (const char*)buffer, n, 0, (const struct sockaddr*) &cliaddr, msg.msg_namelen) < 0)
	            perror("Cannot send message");

                count++;
                if(count % 100 == 0)
                    std::cout << count << " messages received. Last one was " << n << " byte long and came from " << inet_ntoa(cliaddr.sin_addr) 
                              << " with ToS/DSCP field set to " << getToS(msg) << std::endl;
            }
            else
            {
                SEQNR_TYPE seqnr;
                memcpy(&seqnr, buffer, sizeof(SEQNR_TYPE));
                received[seqnr].first = now;
                received[seqnr].second = n * 8;
           }
       }       
    }
    return 0;
}
