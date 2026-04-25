#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    bool bind(int port);
    void setNonBlocking(bool nonBlocking);
    
    // Returns bytes read. -1 on error, 0 if no data (when non-blocking)
    int receive(char* buffer, int maxLen);
    
    // Send data to destination
    bool sendTo(const std::string& ip, int port, const std::string& data);

private:
    int sockfd = -1;
};
