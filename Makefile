CXXFILES:=$(wildcard src/*.cpp)
OFILES=$(CXXFILES:.cpp=.o)
TARGET=openvrrp
CXX:=g++
EXTRA_CXXFLAGS=
EXTRA_LDFLAGS=

all: $(TARGET)

$(TARGET): $(OFILES)
	$(CXX) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $(TARGET) $(OFILES)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c -o $@ $^

clean:
	rm -f $(OFILES) $(TARGET)

.PHONY: all clean
