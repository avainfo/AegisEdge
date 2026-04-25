#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

struct DroneState {
	double x;
	double y;
	double vx;
	double vy;
	double roll;
	double pitch;
	double yaw;
	double altitude;
};

int main (int argc, char *argv[]) {
const char* target_ip = "127.0.0.1";
    const int target_port = 5005;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_ip, &target.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock);
        return 1;
    }
	return 0;
}
