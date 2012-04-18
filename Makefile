CXXFILES:=$(wildcard src/*.cpp)
OFILES=$(CXXFILES:.cpp=.o)
TARGET=openvrrp
CXX:=g++
EXTRA_CXXFLAGS=-std=c++0x -g
EXTRA_LDFLAGS=-g

all: $(TARGET)

$(TARGET): $(OFILES)
	$(CXX) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $(TARGET) $(OFILES)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c -o $@ $^

clean:
	rm -f $(OFILES) $(TARGET)

test: $(TARGET)
	sudo ./$(TARGET)

debug: $(TARGET)
	sudo gdb ./$(TARGET)

.PHONY: all clean test debug
