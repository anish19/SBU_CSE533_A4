CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a

FLAGS = -g -O2 -I/home/courses/cse533/Stevens/unpv13e/lib

all: arp tour prhwaddrs_aa19

arp: arp.o get_hw_addrs_aa19.o
	${CC} -g -o $@ arp.o get_hw_addrs_aa19.o ${LIBS}

arp.o: arp.c
	${CC} ${FLAGS} -c arp.c

tour: tour.o 
	${CC} -g -o $@ tour.o  ${LIBS}

tour.o: tour.c
	${CC} ${FLAGS} -c tour.c

prhwaddrs_aa19: prhwaddrs_aa19.o get_hw_addrs_aa19.o
	${CC} -g -o prhwaddrs_aa19 prhwaddrs_aa19.o get_hw_addrs_aa19.o ${LIBS}


get_hw_addrs_aa19.o: get_hw_addrs_aa19.c
	${CC} ${FLAGS} -c get_hw_addrs_aa19.c

prhwaddrs_aa19.o: prhwaddrs_aa19.c
	${CC} ${FLAGS} -c prhwaddrs_aa19.c



clean:
	rm *.o arp prhwaddrs_aa19 tour
