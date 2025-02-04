#ifndef __UDP_PING_H
#define __UDP_PING_H

#define SEQNR_TYPE long int
#define MAXBUF 70000 // Enabling Jumbo-frames
#define PORT 1234

struct paramsType {
    std::string host_name;
    int port;
    int num_packets;
    int packet_size;
    int tos;
    float interval;
    int mode;
    float ratio;
    bool timestamp;
    bool throughput;
    bool live;
    int changePort;
};

#endif
