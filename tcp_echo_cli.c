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

#define MAX_CMD_STR 100

int sig_type = 0;
FILE *fp_res = NULL;

void sig_pipe(int signo) {
  sig_type = signo;
  printf("SIGINT is coming!");
}

uint8_t *write_bytes(void *dst, void *src, uint32_t size) {
  memcpy(dst, src, size);
  return (uint8_t *)(dst) + size;
}

int echo_rqt(int sockfd, int pin) {
  uint32_t pin_n = htonl(pin);
  char filename_td[20];
  sprintf(filename_td, "fd%d.txt", pin);
  FILE *fp_td = fopen(filename_td, "r");
  if (!fp_td) {
    LOG(fp_res, "Test data read error!");
    return 0;
  }
  uint8_t buf[10 + MAX_CMD_STR];
  char buf_read[1 + MAX_CMD_STR];
  memset(buf, 0, sizeof(buf));
  while (fgets(buf_read, MAX_CMD_STR, fp_td)) {
    if (buf_read[0] == '\n') buf_read[0] = '\0';
    int len = strnlen(buf_read, MAX_CMD_STR);
    int len_n = htonl(len ? len : 1);
    uint8_t *p = buf;
    p = write_bytes(p, &pin_n, 4);
    p = write_bytes(p, &len_n, 4);
    p = write_bytes(p, buf_read, len + 1);
    // refill
    memset(buf, 0, sizeof(buf));
    read(sockfd, &pin_n, 4);
    read(sockfd, &len_n, 4);
    len = ntohl(len_n);
    int read_bytes = 0;
    do {
      read_bytes += read(sockfd, buf + read_bytes, len - read_bytes);
      if (read_bytes == len) break;
      if (read_bytes > len) {
        LG("ERROR");
        return pin;
      }
    } while (1);
    LOG_REP(fp_res, "%s", buf);
  }
  return 0;
}

int process_socket(struct sockaddr_in *addr_server, int pin) {
  char filename[32];
  sprintf(filename, "stu_cli_res_%d.txt", pin);
  fp_res = fopen(filename, "ab");
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
  setup_signal_handler(SIGPIPE, sig_pipe, SA_RESTART);
  setup_signal_handler(SIGCHLD, SIG_IGN, SA_RESTART);

  int concurrent = atoi(argv[3]);
  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_port = htons(atoi(argv[2]))};
  inet_pton(AF_INET, argv[1], &addr_server.sin_addr);

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