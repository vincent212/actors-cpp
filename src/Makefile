LIBSRC = Actor.cpp Manager.cpp Group.cpp
NAM = actors

CXX = g++
CXXFLAGS = -std=c++20 -O2 -I../include
LDFLAGS = -lpthread

# Remote actor support (ZMQ + JSON)
REMOTE_LDFLAGS = -lzmq

OBJS = $(LIBSRC:.cpp=.o)
LIB = lib$(NAM).a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Example targets
examples: ../examples/ping_pong ../examples/remote_pong ../examples/remote_ping

../examples/ping_pong: ../examples/ping_pong.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< -o $@ -L. -l$(NAM) $(LDFLAGS)

../examples/remote_pong: ../examples/remote_pong.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< -o $@ -L. -l$(NAM) $(LDFLAGS) $(REMOTE_LDFLAGS)

../examples/remote_ping: ../examples/remote_ping.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< -o $@ -L. -l$(NAM) $(LDFLAGS) $(REMOTE_LDFLAGS)

clean:
	rm -f $(OBJS) $(LIB) ../examples/ping_pong ../examples/remote_pong ../examples/remote_ping

.PHONY: all clean examples
