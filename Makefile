CC = gcc

all:
	${CC} -o utils.o -c utils.c -pthread
	${CC} -o reliable_com.o -c reliable_com.c -pthread
	${CC} -o connection.o -c connection.c -pthread
	${CC} -o protocol.o -c protocol.c -pthread
	${CC} -o file_ops.o -c file_ops.c -pthread
	${CC} client_src/client_udp.c -o client file_ops.o protocol.o connection.o reliable_com.o utils.o -pthread
	${CC} server_src/server_udp.c -o server file_ops.o protocol.o connection.o reliable_com.o utils.o -pthread

debug:
	${CC} -o debug.o -c debug.c -pthread -Ddebug
	${CC} -o utils.o -c utils.c -pthread
	${CC} -o reliable_com.o -c reliable_com.c -pthread
	${CC} -o connection.o -c connection.c -pthread
	${CC} -o protocol.o -c protocol.c -pthread
	${CC} -o file_ops.o -c file_ops.c -pthread
	${CC} client_src/client_udp.c -o client file_ops.o protocol.o connection.o reliable_com.o utils.o debug.o -pthread -Ddebug
	${CC} server_src/server_udp.c -o server file_ops.o protocol.o connection.o reliable_com.o utils.o debug.o -pthread -Ddebug

clean:
	-rm debug.o
	-rm utils.o
	-rm reliable_com.o
	-rm connection.o
	-rm protocol.o
	-rm file_ops.o
	-rm server
	-rm client
