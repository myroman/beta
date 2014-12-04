include Make.defines


all: get_hw_addrs.o prhwaddrs.o tour arp
	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c
misc.o: misc.c
	${CC} ${FLAGS} -c misc.c ${UNP}

#get_ifi_info_plus.o: get_ifi_info_plus.c
#	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

tour: tour.o get_hw_addrs.o misc.o ping.o
	${CC} ${FLAGS} -o $@ tour.o get_hw_addrs.o misc.o ping.o ${LIBS}

arp: arp.o get_hw_addrs.o cacheEntry.o misc.o
	${CC} ${FLAGS} -o $@ arp.o get_hw_addrs.o cacheEntry.o misc.o ${LIBS}


misc.o: misc.c
	${CC} ${FLAGS} -c misc.c ${UNP}
ping.o: ping.c
	${CC} ${FLAGS} -c ping.c ${UNP}	

cacheEntry.o: cacheEntry.c misc.o
	${CC} ${FLAGS} -c cacheEntry.c misc.o ${UNP}

clean:
	rm *.o tour 
