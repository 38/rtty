CC=android_gcc
HOSTCC=gcc
CFLAGS=-DTARGET_ANDROID -g
HCFLAGS=-DTARGET_LINUX -g
all: rttyd rtty

rttyd: rttyd.o pts.o network.o protocol.o
	$(CC) $^ -o $@
rttyd.o: rttyd.c pts.h network.h
	$(CC) -c $(CFLAGS) $< -o $@
pts.o: pts.c
	$(CC) -c $(CFLAGS) $< -o $@
network.o: network.c
	$(CC) -c $(CFLAGS) $< -o $@
rtty : rtty.o protocol_host.o pts_host.o 
	$(HOSTCC) $^ -o $@
rtty.o: rtty.c
	$(HOSTCC) -c $(HCFLAGS) $< -o $@ 
pts_host.o: pts.c
	$(HOSTCC) -c $(HCFLAGS) $< -o $@
protocol.o: protocol.c
	$(CC) -c $(CFLAGS) $< -o $@
protocol_host.o: protocol.c
	$(HOSTCC) -c $(HCFLAGS) $< -o $@

clean: 
	rm -f *.o
