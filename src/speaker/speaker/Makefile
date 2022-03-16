OBJECTS = speaker.o

all: speaker

speaker.o: speaker.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

speaker: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f speaker
	rm -f $(OBJECTS)
