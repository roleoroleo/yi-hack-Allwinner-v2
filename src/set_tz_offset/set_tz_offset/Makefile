OBJECTS = set_tz_offset.o
LIBS = -lpthread -lrt

all: set_tz_offset

set_tz_offset.o: set_tz_offset.c $(HEADERS)
	$(CC) -c $< -fPIC -O2 -o $@

set_tz_offset: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f set_tz_offset
	rm -f $(OBJECTS)
