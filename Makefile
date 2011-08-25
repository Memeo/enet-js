NODE_PREFIX = /usr/local
CXX = g++
CFLAGS = 
NODE_CFLAGS := -g -O2 -fPIC -DPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DEV_MULTIPLICITY=0 -I$(NODE_PREFIX)/include/node
CCLD = $(CXX)
LDFLAGS = 
NODE_LDFLAGS = 
LIBS = -lenet

all: enet.node
    
enetnat.node: enetnat.o
	$(CCLD) $(LDFLAGS) $(NODE_LDFLAGS) $(LIBS) -shared -o enetnat.node enetnat.o
    
enetnat.o: enet.cc
	$(CXX) $(CFLAGS) $(NODE_CFLAGS) -c -o enetnat.o enet.cc
    
