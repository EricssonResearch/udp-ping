#ifndef __UDP_PING_H
#define __UDP_PING_H

#define SEQNR_TYPE long int
#define MAXBUF 1600
#define PORT 1234

struct paramsType {
    const char * host_name;
    int port;
    int num_packets;
    int packet_size;
    int interval;
    int mode;
    float ratio;
    bool timestamp;
};

#endif
