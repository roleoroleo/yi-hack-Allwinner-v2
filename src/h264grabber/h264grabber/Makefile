OBJECTS = h264grabber.o
OPTS = -mcpu=cortex-a7 -mfpu=neon-vfpv4

CC= arm-openwrt-linux-gcc
STRIP= arm-openwrt-linux-strip

all: h264grabber

h264grabber.o: h264grabber.c $(HEADERS)
	$(CC) -c $< $(OPTS) -fPIC -O2 -o $@

h264grabber: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIB) $(OPTS) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f h264grabber
	rm -f $(OBJECTS)
