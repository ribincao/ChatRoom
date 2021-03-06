CC = g++
CFLAGS = -std=c++11
 
all: Client_Main.cpp Server_Main.cpp Server.o Client.o
	$(CC) $(CFLAGS) Server_Main.cpp  Server.o -o Srv
	$(CC) $(CFLAGS) Client_Main.cpp Client.o -o Cli
 
Server.o: Server.cpp Server.h Base.h
	$(CC) $(CFLAGS) -c Server.cpp
 
Client.o: Client.cpp Client.h Base.h
	$(CC) $(CFLAGS) -c Client.cpp
 
clean:
	rm -f *.o Srv Cli