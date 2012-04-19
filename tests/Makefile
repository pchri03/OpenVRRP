all: test-app netlink-test-app

TESTLINE=./netlink-test-app

test: netlink-test-app
	sudo $(TESTLINE)

debug: netlink-test-app
	sudo gdb $(TESTLINE)

test-app: test.cpp
	g++ -Wall -W -g -o test-app test.cpp

netlink-test-app: netlink-test.cpp
	g++ -Wall -W -g -o netlink-test-app netlink-test.cpp

.PHONY: test all
