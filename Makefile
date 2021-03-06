include Make.defines


all: testing get_hw_addrs.o prhwaddrs.o tour arp 
	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c
aapi.o: aapi.c 
	${CC} ${FLAGS} -c aapi.c ${UNP}	
#get_ifi_info_plus.o: get_ifi_info_plus.c
#	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

tour: tour.o get_hw_addrs.o aapi.o misc.o ping.o checksum.o
	${CC} ${FLAGS} -o $@ tour.o get_hw_addrs.o aapi.o misc.o ping.o checksum.o ${LIBS}

arp: arp.o get_hw_addrs.o cacheEntry.o misc.o
	${CC} ${FLAGS} -o $@ arp.o get_hw_addrs.o cacheEntry.o misc.o ${LIBS}

misc.o: misc.c
	${CC} ${FLAGS} -c misc.c ${UNP}
ping.o: ping.c
	${CC} ${FLAGS} -c ping.c ${UNP}	
checksum.o: checksum.c
	${CC} ${FLAGS} -c checksum.c ${UNP}		

cacheEntry.o: cacheEntry.c misc.o
	${CC} ${FLAGS} -c cacheEntry.c misc.o ${UNP}

testing: testing.o
	${CC} ${FLAGS} -o $@ testing.o ${LIBS}

clean:
	rm *.o tour arp
