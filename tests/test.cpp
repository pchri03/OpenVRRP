#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>


int main ()
{
	static const char *interface = "lo";

	// Create socket
	int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1)
	{
		perror("socket");
		return -1;
	}

	// Bind to IP
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(12999);
	inet_pton(AF_INET, "192.168.1.121", &addr.sin_addr);
	if (bind(s, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		perror("bind");
		return -1;
	}

	// Bind to device
	if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) == -1)
	{
		perror("setsockopt(SO_BINDTODEVICE)");
		return -1;
	}

	// Join multicast
	ip_mreqn req;
	inet_pton(AF_INET, "239.192.2.1", &req.imr_multiaddr);
	req.imr_address.s_addr = addr.sin_addr.s_addr;
	req.imr_ifindex = if_nametoindex(interface);
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) == -1)
	{
		perror("setsockopt(IP_ADD_MEMBERSHIP");
		return -1;
	}

	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	for (;;)
	{
		int count = select(s + 1, &set, NULL, NULL, &timeout);
		if (count == 0)
		{
			addr.sin_family = AF_INET;
			inet_pton(AF_INET, "239.192.2.1", &addr.sin_addr);
			addr.sin_port = htons(12999);

			if (sendto(s, "Hello world", strlen("Hello world"), 0, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
				perror("sendto");
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			FD_SET(s, &set);
		}
		else
		{
			socklen_t addrlen = sizeof(addr);
			uint8_t buffer[1500];
			ssize_t size = recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
			char addr_buffer[16];

			if (size == -1)
				perror("recvfrom");
			else
				printf("Received %zi bytes from %s:%hu\n", size, inet_ntop(addr.sin_family, &addr.sin_addr, addr_buffer, sizeof(addr_buffer)), ntohs(addr.sin_port));
		}
	}

	close(s);

	return 0;
}
