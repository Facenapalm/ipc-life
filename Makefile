#
# main
#
all: life-server life-client
	
client: life-client
	
server: life-server
	
#
# binary files
#
life-client: client.o
	gcc -m32 -o life-client client.o
life-server: core.o board.o server.o
	gcc -m32 -o life-server core.o board.o server.o
#
# modules
#
core.o: core.c core.h
	gcc -std=c99 -m32 -c -o core.o core.c
board.o: board.c board.h core.h
	gcc -std=c99 -m32 -c -o board.o board.c
client.o: client.c common.h
	gcc -std=c99 -m32 -c -o client.o client.c
server.o: server.c board.h text.h common.h
	gcc -std=c99 -m32 -c -o server.o server.c
#
# cleanings
#
clean-temps:
	rm -f core.o
	rm -f board.o
	rm -f client.o
	rm -f server.o
clean: clean-temps
	rm -f life-server
	rm -f life-client
