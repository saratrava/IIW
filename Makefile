CC = gcc

all:
	${CC} -o sr_udp.o -c sr_udp.c -pthread
	${CC} client_src/client_udp.c -o client sr_udp.o -pthread
	${CC} server_src/server_udp.c -o server sr_udp.o -pthread

debug:
	${CC} -o sr_udp.o -c sr_udp.c -pthread -Ddebug
	${CC} client_src/client_udp.c -o client sr_udp.o -pthread -Ddebug
	${CC} server_src/server_udp.c -o server sr_udp.o -pthread

clean:
	-rm sr_udp.o
	-rm server
	-rm client
