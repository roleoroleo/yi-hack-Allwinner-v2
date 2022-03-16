OBJECTS = pcmvol.o
all: pcmvol

pcmvol.o: pcmvol.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

pcmvol: $(OBJECTS)
	$(CC) $(OBJECTS) -lm -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f pcmvol
	rm -f $(OBJECTS)
