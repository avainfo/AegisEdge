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
	sockaddr_in servaddr;

	int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	return 0;
}
