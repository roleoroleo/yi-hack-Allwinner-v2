OBJECTS = pcmvol.o
all: pcmvol

pcmvol.o: pcmvol.c $(HEADERS)
	$(CC) -c $< -fPIC -O1 -o $@

pcmvol: $(OBJECTS)
	$(CC) $(OBJECTS) -lm -fPIC -O1 -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f pcmvol
	rm -f $(OBJECTS)
