#!/bin/sh

# Shell script to run when becoming master
echo 1 > /proc/sys/net/ipv4/conf/$VRRP_IF/arp_filter

echo 0 > /proc/sys/net/ipv4/conf/$VRRP_VIF/arp_filter
if [ "$VRRP_STATE" = "master" ]; then
	if [ $VRRP_PRIO -eq 255 ] || [ $VRRP_ACCEPT -eq 1 ]; then 
		echo 1 > /proc/sys/net/ipv4/conf/$VRRP_VIF/accept_local
	fi
fi
