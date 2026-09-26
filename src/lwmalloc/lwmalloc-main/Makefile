#CC=gcc
CFLAGS=-fPIC -Wall -Wextra -O2 -DLIB32
LDFLAGS=-shared
TARGET_LIB=liblwmalloc.so
SOURCES=lwmalloc.c
OBJECTS=$(SOURCES:.c=.o)
.PHONY: all clean

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET_LIB) $(OBJECTS)

buildtest:
	$(CC) $(CFLAGS) -o test test.c