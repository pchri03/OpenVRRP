all: test-app netlink-test-app libnl2-test-app

TESTAPP=libnl2-test-app
TESTLINE=./$(TESTAPP)

test: $(TESTAPP)
	sudo $(TESTLINE)

debug: $(TESTAPP)
	sudo gdb $(TESTLINE)

test-app: test.cpp
	g++ -Wall -W -g -o test-app test.cpp

netlink-test-app: netlink-test.cpp
	g++ -Wall -W -g -o netlink-test-app netlink-test.cpp

libnl2-test-app: libnl2-test.cpp ../src/mainloop.cpp
	g++ -Wall -W -g -std=c++0x -o libnl2-test-app -I ../src $^ -lnl -lnl-route

.PHONY: test all
