all: p2

p2: wserver wclient fib.cgi
		g++ wserver.c -o wserver -lpthread
		g++ wclient.c -o wclient
		g++ fib.cpp -o fib.cgi

wclient: wclient.c
		g++ -c wclient.c

wserver: wserver.c
		g++ -c wserver.c

fib.cgi: fib.cpp
		g++ -c fib.cpp

clean:
		rm -f *.o p2