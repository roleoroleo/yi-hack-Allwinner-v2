OBJECTS_C = ipc_cmd.o
OBJECTS_R = ipc_read.o
OBJECTS_M = ipc_multiplexer.o
OBJECTS_N = ipc_notify.o
#LIBS = -lpthread -lrt

all: ipc_cmd ipc_read ipc_multiplexer ipc_notify

ipc_cmd.o: ipc_cmd.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

ipc_read.o: ipc_read.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

ipc_multiplexer.o: ipc_multiplexer.c $(HEADERS)
	$(CC) -c $< -fPIC -Os -o $@

ipc_cmd: $(OBJECTS_C)
	$(CC) $(OBJECTS_C) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

ipc_read: $(OBJECTS_R)
	$(CC) $(OBJECTS_R) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

ipc_multiplexer: $(OBJECTS_M)
	$(CC) $(OBJECTS_M) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

ipc_notify: $(OBJECTS_N)
	$(CC) $(OBJECTS_N) $(LIBS) -fPIC -Os -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f ipc_cmd
	rm -f ipc_read
	rm -f ipc_multiplexer
	rm -f ipc_notify
	rm -f $(OBJECTS_C)
	rm -f $(OBJECTS_R)
	rm -f $(OBJECTS_M)
	rm -f $(OBJECTS_N)
