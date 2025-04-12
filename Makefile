CC = gcc

all:
	${CC} -o sr_udp.o -c sr_udp.c -pthread
	${CC} client/client_udp.c -o clientF sr_udp.o -pthread
	${CC} server/server_udp.c -o serverF sr_udp.o -pthread

debug:
	${CC} -o sr_udp.o -c sr_udp.c -pthread -Ddebug
	${CC} client/client_udp.c -o clientF sr_udp.o -pthread -Ddebug
	${CC} server/server_udp.c -o serverF sr_udp.o -pthread

clean:
	-rm sr_udp.o
	-rm serverF
	-rm clientF
