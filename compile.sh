#!/bin/sh
which pkg-config > /dev/null
if [ $? -ne 0 ]; then
	echo "OpenVRRP requires pkg-config" >&2
	exit 1
fi

LIBNL3=0
LIBNL2=0
pkg-config --exists libnl-2.0 && LIBNL2=1
pkg-config --exists libnl-route-3.0 && LIBNL3=1

if [ "$CXX" = "" ]; then
	CXX=g++
fi

if [ $LIBNL3 -eq 1 ]; then
	$CXX -std=c++0x $CXXFLAGS `pkg-config --cflags libnl-route-3.0` -DLIBNL3 -c -o $*
elif [ $LIBNL2 -eq 1 ]; then
	$CXX -std=c++0x $CXXFLAGS `pkg-config --cflags libnl-2.0` -DLIBNL2-c -o $*
else
	echo "OpenVRRP requires libnl version 3 or 2" >&2
	exit 1
fi
