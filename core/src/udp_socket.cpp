#include "udp_socket.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>

UdpSocket::UdpSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
    }
}

UdpSocket::~UdpSocket() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

bool UdpSocket::bind(int port) {
    if (sockfd < 0) return false;

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (::bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Bind failed on port " << port << std::endl;
        return false;
    }
    return true;
}

void UdpSocket::setNonBlocking(bool nonBlocking) {
    if (sockfd < 0) return;
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (nonBlocking) {
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
    }
}

int UdpSocket::receive(char* buffer, int maxLen) {
    if (sockfd < 0) return -1;
    
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    
    int n = recvfrom(sockfd, buffer, maxLen, 0, (struct sockaddr *) &cliaddr, &len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
    }
    return n;
}

bool UdpSocket::sendTo(const std::string& ip, int port, const std::string& data) {
    if (sockfd < 0) return false;

    struct sockaddr_in destaddr;
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &destaddr.sin_addr);

    int n = sendto(sockfd, data.c_str(), data.length(), 0, (const struct sockaddr *) &destaddr, sizeof(destaddr));
    return n == (int)data.length();
}
