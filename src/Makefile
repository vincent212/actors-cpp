LIBSRC = Actor.cpp Manager.cpp Group.cpp
NAM = actors

CXX = g++
CXXFLAGS = -std=c++20 -O2 -I../include
LDFLAGS = -lpthread

OBJS = $(LIBSRC:.cpp=.o)
LIB = lib$(NAM).a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)

.PHONY: all clean
