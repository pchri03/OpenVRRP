CXXFILES:=$(wildcard src/*.cpp)
OFILES=$(CXXFILES:.cpp=.o)
TARGET=openvrrp
CXX:=g++
EXTRA_CXXFLAGS=-std=c++0x
EXTRA_LDFLAGS=

all: $(TARGET)

$(TARGET): $(OFILES)
	$(CXX) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $(TARGET) $(OFILES) -lnl -lnl-route

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c -o $@ $^

clean:
	rm -f $(OFILES) $(TARGET)

test: $(TARGET)
	sudo ./$(TARGET)

debug: $(TARGET)
	sudo gdb ./$(TARGET)

.PHONY: all clean test debug
