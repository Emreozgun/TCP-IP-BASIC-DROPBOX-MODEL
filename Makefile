all:
	gcc BibakBOXServer.c -o server ServerClient.c -lpthread -lm
	gcc BibakBOXClient.c -o client ServerClient.c -lpthread -lm

clean:
	rm -f *.o BibakBOXClient BibakBOXServer *.log
