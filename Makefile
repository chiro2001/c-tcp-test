all: clean server client tar
server:
	gcc -o server tcp_echo_srv.c
client:
	gcc -o client tcp_echo_cli.c
run: all
	./server 127.0.0.0 9999 &
	sleep 0.5
	./client 127.0.0.0 9999 3
	sleep 0.5
	killall server
tar:
	tar cvf src.tar tcp_echo_cli.c tcp_echo_srv.c
clean:
	-rm stu*.txt server client