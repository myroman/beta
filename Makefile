include Make.defines


all: get_hw_addrs.o prhwaddrs.o tour
	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c
misc.o: misc.c
	${CC} ${FLAGS} -c misc.c ${UNP}

#get_ifi_info_plus.o: get_ifi_info_plus.c
#	${CC} ${CFLAGS} -c get_ifi_info_plus.c ${UNP}

tour: tour.o get_hw_addrs.o misc.o
	${CC} ${FLAGS} -o $@ tour.o get_hw_addrs.o misc.o ${LIBS}

clean:
	rm *.o tour 
