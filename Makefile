all: clean server client
server:
	gcc tcp_echo_srv.c -o server
client:
	gcc tcp_echo_cli.c -o client
run: all
	./server 127.0.0.0 9999 &
	sleep 0.5
	./client 127.0.0.0 9999 3
	sleep 0.5
	-killall server
tar:
	tar cvf src.tar tcp_echo_cli.c tcp_echo_srv.c
clean:
	-rm -rf stu*.txt
	-rm -rf server
	-rm -rf client
	-killall server
	-killall client
	sleep 0.5