#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#define TEST_DELLINK

int main ()
{
	int s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (s == -1)
	{
		perror("socket");
		return -1;
	}

	sockaddr_nl addr;
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;	// link, ipv4 addr, ipv6 addr
	if (bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
		return -1;

	uint8_t buffer[1024];
	memset(buffer, 0, sizeof(buffer));

	// ifaddrmsg
	// IFA_ADDRESS
	// IFA_F_SECONDARY

	// See RFC 3549
	
	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer);
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

#if defined(TEST_NEWADDR) || defined(TEST_DELADDR)
	hdr->nlmsg_len = 16 + 8 + 8;
#ifdef TEST_NEWADDR
	hdr->nlmsg_type = RTM_NEWADDR;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
#else // TEST_NEWADDR
	hdr->nlmsg_type = RTM_DELADDR;
	hdr->nlmsg_flags = NLM_F_REQUEST;
#endif // TEST_NEWADDR
#endif // TEST_NEWADDR || TEST_DELADDR

#if defined(TEST_NEWADDR) || defined(TEST_DELADDR)
	ifaddrmsg *msg = reinterpret_cast<ifaddrmsg *>(buffer + 16);
	msg->ifa_family = AF_INET;
	msg->ifa_prefixlen = 32;
	msg->ifa_flags = 0;
	msg->ifa_scope = RT_SCOPE_LINK;
	msg->ifa_index = if_nametoindex("eth0");

	rtattr *attr = reinterpret_cast<rtattr *>(buffer + 16 + 8);
	attr->rta_len = 8;
	attr->rta_type = IFA_LOCAL;

	inet_pton(AF_INET, "192.168.4.1", buffer + 16 + 8 + 4);
#elif defined(TEST_NEWLINK)
	hdr->nlmsg_type = RTM_NEWLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = ARPHRD_ETHER;
	msg->ifi_index = 0;
	msg->ifi_flags = 0;
	msg->ifi_change = 0;

	uint8_t *ptr = buffer + 16 + 16;

	rtattr *attr = reinterpret_cast<rtattr *>(ptr);
	attr->rta_len = 10;
	attr->rta_type = IFLA_ADDRESS;
	static const uint8_t mac[6] = {0x00, 0x00, 0x5E, 0x00, 0x01, 0x01};
	memcpy(ptr + 4, mac, 6);
	memset(ptr + 10, 0, 2);
	ptr += 12;

	attr = reinterpret_cast<rtattr *>(ptr);
	attr->rta_len = 8;
	attr->rta_type = IFLA_LINK;
	*reinterpret_cast<uint32_t *>(ptr + 4) = if_nametoindex("eth0");
	ptr += 8;

	attr = reinterpret_cast<rtattr *>(ptr);
	attr->rta_len = 28;
	attr->rta_type = IFLA_LINKINFO;
	ptr += 4;
	{
		attr = reinterpret_cast<rtattr *>(ptr);
		attr->rta_len = 12;
		attr->rta_type = IFLA_INFO_KIND;
		memcpy(ptr + 4, "macvlan", 8);
		ptr += 12;

		attr = reinterpret_cast<rtattr *>(ptr);
		attr->rta_len = 12;
		attr->rta_type = IFLA_INFO_DATA;
		ptr += 4;
		{
			attr = reinterpret_cast<rtattr *>(ptr);
			attr->rta_len = 8;
			attr->rta_type = IFLA_MACVLAN_MODE;
			*reinterpret_cast<uint32_t *>(ptr + 4) = MACVLAN_MODE_PRIVATE;
			ptr += 8;
		}
	}

	hdr->nlmsg_len = ptr - buffer;
#elif defined(TEST_DELLINK)
	hdr->nlmsg_len = 32;
	hdr->nlmsg_type = RTM_DELLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST;

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = 0;
	msg->ifi_index = if_nametoindex("macvlan0");
	msg->ifi_flags = 0;
	msg->ifi_change = 0;

#endif // TEST_NEWADDR || TEST_DELADDR || TEST_NEWLINK

	if (write(s, buffer, hdr->nlmsg_len) == -1)
	{
		perror("write");
		return -1;
	}

	do
	{
		memset(buffer, 0, sizeof(buffer));
		unsigned int size = read(s, buffer, sizeof(buffer));
		if (size == (unsigned int)-1)
		{
			perror("read");
			return -1;
		}

		printf("Received %i bytes\n", size);

		if (hdr->nlmsg_type == NLMSG_ERROR)
		{
			const nlmsgerr *err = reinterpret_cast<const nlmsgerr *>(buffer + 16);
			printf("Got error %i (%s)\n", err->error, strerror(0 - err->error));
		}
	} while (hdr->nlmsg_flags & NLM_F_MULTI && hdr->nlmsg_type != NLMSG_DONE);

	close(s);

	return 0;
}

