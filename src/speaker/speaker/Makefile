OBJECTS = speaker.o

all: speaker

speaker.o: speaker.c $(HEADERS)
	$(CC) -c $< -fPIC -O2 -o $@

speaker: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -fPIC -O2 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f speaker
	rm -f $(OBJECTS)
