CC = gcc

all:
	${CC} -o sr_udp.o -c sr_udp.c -pthread
	${CC} -o types.o -c types.c -pthread
	${CC} -o utils.o -c utils.c -pthread
	${CC} -o socket_comm.o -c socket_comm.c -pthread
	${CC} -o handshake.o -c handshake.c -pthread
	${CC} -o protocol.o -c protocol.c -pthread
	${CC} -o file_ops.o -c file_ops.c -pthread
	${CC} client_src/client_udp.c -o client sr_udp.o -pthread
	${CC} server_src/server_udp.c -o server sr_udp.o -pthread

debug:
	${CC} -o sr_udp.o -c sr_udp.c -pthread -Ddebug
	${CC} -o types.o -c types.c -pthread
	${CC} -o utils.o -c utils.c -pthread
	${CC} -o socket_comm.o -c socket_comm.c -pthread
	${CC} -o handshake.o -c handshake.c -pthread
	${CC} -o protocol.o -c protocol.c -pthread
	${CC} -o file_ops.o -c file_ops.c -pthread
	${CC} client_src/client_udp.c -o client sr_udp.o -pthread -Ddebug
	${CC} server_src/server_udp.c -o server sr_udp.o -pthread

clean:
	-rm sr_udp.o
	-rm types.o
	-rm utils.o
	-rm socket_comm.o
	-rm handshake.o
	-rm protocol.o
	-rm file_ops.o
	-rm server
	-rm client
