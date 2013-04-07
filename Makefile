CXXFILES:=$(wildcard src/*.cpp)
OFILES=$(CXXFILES:.cpp=.o)
TARGET=openvrrp

all: $(TARGET)

$(TARGET): $(OFILES)
	sh link.sh $(TARGET) $(OFILES)

%.o: %.cpp
	sh compile.sh $@ $^

clean:
	rm -f $(OFILES) $(TARGET)

test: $(TARGET)
	sudo ./$(TARGET)

debug: $(TARGET)
	sudo gdb ./$(TARGET)

.PHONY: all clean test debug
