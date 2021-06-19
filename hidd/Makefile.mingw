CC=c++
CFLAGS=-Wall $(shell pkg-config --cflags libusb hidapi) -fpermissive -std=c++17
LDFLAGS=$(shell pkg-config --libs libusb-1.0 hidapi) -lwinmm -lole32 -luuid -lsetupapi -static

hid: hid.cpp
	$(CC) $(CFLAGS) -o hid hid.cpp $(LDFLAGS)

clean:
	rm -f hid
