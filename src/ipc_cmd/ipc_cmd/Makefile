OBJECTS_C = ipc_cmd.o
OBJECTS_R = ipc_read.o
OBJECTS_N = ipc_notify.o
#LIBS = -lpthread -lrt

all: ipc_cmd ipc_read ipc_notify ipc_multiplex.so

ipc_cmd.o: ipc_cmd.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

ipc_read.o: ipc_read.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

ipc_multiplex.so: ipc_multiplex.c $(HEADERS)
	$(CC) $< -fPIC -Os -shared -o $@

ipc_cmd: $(OBJECTS_C)
	$(CC) $(OBJECTS_C) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

ipc_read: $(OBJECTS_R)
	$(CC) $(OBJECTS_R) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

ipc_notify: $(OBJECTS_N)
	$(CC) $(OBJECTS_N) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f ipc_cmd
	rm -f ipc_read
	rm -f ipc_notify
	rm -f ipc_multiplex.so
	rm -f $(OBJECTS_C)
	rm -f $(OBJECTS_R)
	rm -f $(OBJECTS_N)
