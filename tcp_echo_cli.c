#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "public.h"

int sig_type = 0;
int sig_to_exit = 0;

void sig_pipe(int signo) {
  sig_type = signo;
  printf("[cli](%d) SIGINT is coming!\n", getpid());
  sig_to_exit = 1;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>\n", argv[0]);
    return 0;
  }
  setup_signal_handler(SIGPIPE, sig_pipe);
  setup_signal_handler(SIGCHLD, SIG_IGN);

  int concurrent = atoi(argv[3]);
  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_port = htons(atoi(argv[2]))};
  inet_pton(AF_INET, argv[1], &addr_server.sin_addr);

  pid_t pid_parent = getpid();

  for (int fork_index = 1; fork_index < concurrent; fork_index++) {
    pid_t pid_child = fork();
    LG("pid_child = %d", pid_child);
  }
}