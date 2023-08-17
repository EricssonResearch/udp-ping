// Server side implementation of UDP client-server model
#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 1234
#define MAXBUF 1600    // Typically, 1500 Byte is the maximum packet size, but this is of course risky

int main(int argc, char *argv[])
{
    int port, packet_size;
    long int count = 0;

    if(argc > 2)
    {
        std::cout << "\nToo many input parameters\n\n";
        std::cout << "How to use udpServer:\n";
        std::cout << argv[0] << "\t";
        std::cout << "[PORT]\nDefault port number is " << PORT << "\n";
        std::cout << "Terminating the program\n\n";

        return -1;
    }
    if(argc == 2)
        std::istringstream ss1(argv[1]);
    if(argc == 1)
        port = PORT;

    std::cout << "Port: " << port << "\n";

    int sockfd;
    char buffer[MAXBUF];
    struct sockaddr_in servaddr, cliaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) 
    {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
    }

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // Bind the socket with the server address
    if(bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Binding to socket failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len;
    int n = 0;

    while(1)
    {
        len = sizeof(cliaddr);

        n = recvfrom(sockfd, (char*)buffer, MAXBUF, 0, ( struct sockaddr*) &cliaddr, &len);
        sendto(sockfd, (const char*)buffer, n, 0, (const struct sockaddr*) &cliaddr, len);
        count++;
        if(count % 100 == 0)
            std::cout << count << " reply messages sent. Last one was " << n << " byte long." << std::endl;
    }
    return 0;
}
