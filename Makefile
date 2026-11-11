OBJS = main.cpp.o \
       project.cpp.o \
       depMan.cpp.o \
       builder.cpp.o \
       paths.cpp.o \
       json.cpp.o

OUT     = cppkg
CXX    = g++
CC      = gcc
CC_ACCEL = ccache
BUILD_CXX_FLAGS	 = -Wall -std=c++20 -g -flto -fuse-linker-plugin
BULID_CC_FLAGS   =
LINK_OPTS	 =  -flto -fuse-linker-plugin -lcurl

all: $(OBJS)
	$(CC_ACCEL) $(CXX) $(OBJS) $(BUILD_CXX_FLAGS) -g -o $(OUT) $(LINK_OPTS)

%.cpp.o: %.cpp
	$(CC_ACCEL) $(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

%.c.o: %.c
	$(CC_ACCEL) $(CC) $< $(BUILD_CC_FLAGS) -c -o $@

clean:
	rm -f $(OBJS) $(OUT) $(AUTOGENS)

install:
	cp cppkg /usr/local/bin
