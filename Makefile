all: server client
server:
	gcc -o server tcp_echo_srv.c
client:
	gcc -o client tcp_echo_cli.c
run: all
	./server 127.0.0.0 9999 &
	sleep 0.5
	./client 127.0.0.0 9999 10
	sleep 0.5
	killall server