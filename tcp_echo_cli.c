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

#define SELF_CLI 1
#include "public.h"

int sig_type = 0;
int sig_to_exit = 0;

void sig_pipe(int signo) {
  sig_type = signo;
  printf("[cli](%d) SIGINT is coming!\n", getpid());
  sig_to_exit = 1;
}

int echo_rqt(int sockfd, int pin) {
  return 1;
}

int process_socket(struct sockaddr_in *addr_server, int pin) {
  char filename[32];
  sprintf(filename, "stu_cli_res_%d.txt", pin);
  FILE *fp_res = fopen(filename, "ab");
  if (!fp_res) {
    LG("child exits, failed to open file \"stu_cli_res_%d.txt\"!", pin);
    return -1;
  }
  LG("stu_cli_res_%d.txt is created!", pin);
  LOG(fp_res, "child process %d is created!", pin);
  int connfd = socket(AF_INET, SOCK_STREAM, 0);
  if (connfd == -1 && errno == EINTR && sig_type == SIGINT) {
    return 0;
  }
  do {
    int res = connect(connfd, (struct sockaddr *)addr_server,
                      sizeof(struct sockaddr));
    if (!res) {
      char ip[20];
      inet_ntop(AF_INET, &addr_server->sin_addr, ip, sizeof(ip));
      LOG(fp_res, "server[%s:%d] is connected!", ip,
          ntohs(addr_server->sin_port));
      if (!echo_rqt(connfd, pin)) break;
    }
  } while (1);
  close(connfd);
  LOG(fp_res, "connfd is closed!");
  LOG(fp_res, "child process is going to exit!");
  fclose(fp_res);
  LG("stu_cli_res_%d.txt is closed!", pin);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    LG("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>", argv[0]);
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

  for (int pin = 1; pin < concurrent; pin++)
    if (!fork()) {
      int r;
      while ((r = process_socket(&addr_server, pin)) == 0) {
        LG("retry...");
      }
      if (r < 0) exit(r);
      exit(0);
    }
  process_socket(&addr_server, 0);
}