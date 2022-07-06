#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG 1024

#include "public.h"

int sig_to_exit = 0;
int sig_type = 0;
FILE *fp_res = NULL;

void handle_signal(int signo, const char *type) {
  sig_type = signo;
  LOG(fp_res, "%s is coming!\n", type);
  sig_to_exit = 1;
}

void sig_int(int signo) { handle_signal(signo, "SIGINT"); }
void sig_pipe(int signo) { handle_signal(signo, "SIGPIPE"); }

uint8_t *write_bytes(void *dst, void *src, uint32_t size) {
  memcpy(dst, src, size);
  return (uint8_t *)(dst) + size;
}

int echo_rqt(int sockfd) {
  // uint32_t pin_n = htonl(pin);
  // char fn_td[20];
  // sprintf("fd%d.txt", pin);
  // FILE *fp_td = fopen(fn_td, "r");
  // if (!fp_td) {
  //   LOG(fp_res, "Test data read error!");
  //   return 0;
  // }
  // uint8_t buf[10 + MAX_CMD_STR];
  // char buf_read[1 + MAX_CMD_STR];
  // memset(buf, 0, sizeof(buf));
  // while (fgets(buf_read, MAX_CMD_STR, fp_td)) {
  //   if (buf_read[0] == '\n') buf_read[0] = '\0';
  //   int len = strnlen(buf_read, MAX_CMD_STR);
  //   int len_n = htonl(len ? len : 1);
  //   uint8_t *p = buf;
  //   p = write_bytes(p, &pin_n, 4);
  //   p = write_bytes(p, &len_n, 4);
  //   p = write_bytes(p, buf_read, len + 1);
  //   // refill
  //   memset(buf, 0, sizeof(buf));
  //   read(sockfd, &pin_n, 4);
  //   read(sockfd, &len_n, 4);
  //   len = ntohl(len_n);
  //   int read_bytes = 0;
  //   do {
  //     read_bytes += read(sockfd, buf + read_bytes, len - read_bytes);
  //     if (read_bytes == len) break;
  //     if (read_bytes > len) {
  //       LG("ERROR");
  //       return pin;
  //     }
  //   } while (1);
  //   LOG_REP(fp_res, "%s", buf);
  // }
  // return 0;
}

void process_socket(struct sockaddr_in *addr_client, int listenfd, int connfd) {
  char filename[32];
  sprintf(filename, "stu_srv_res_%d.txt", getpid());
  fp_res = fopen(filename, "wb");
  if (!fp_res) {
    LG("child exits, failed to open file \"stu_cli_res_%d.txt\"!", getpid());
    exit(-1);
  }
  LG("stu_cli_res_%d.txt is opened!", getpid());
  LOG(fp_res, "child process is created!");

  close(listenfd);
  LOG(fp_res, "listenfd is closed!");

  int pin = echo_rqt(connfd);
  if (pin < 0) {
    LOG(fp_res, "child exits, client PIN returned by echo_rqt() error!");
    exit(-1);
  }
  char filename_new[20];
  sprintf(filename_new, "stu_srv_res_%d.txt", pin);
  if (!rename(filename, filename_new)) {
    LOG(fp_res, "res file rename done!");
  } else {
    LOG(fp_res, "child exits, res file rename failed!");
  }
  close(connfd);
  LOG(fp_res, "connfd is closed!");
  LOG(fp_res, "child process is going to exit!", getpid());
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    LG("Usage:%s <IP> <PORT>", argv[0]);
    return 0;
  }
  setup_signal_handler(SIGPIPE, sig_pipe, SA_RESTART);
  setup_signal_handler(SIGCHLD, SIG_IGN, SA_RESTART);
  setup_signal_handler(SIGPIPE, sig_int, 0);

  fp_res = fopen("stu_srv_res_p.txt", "wb");
  if (!fp_res) {
    LG("failed to open file \"stu_srv_res_p.txt\"!");
    return 1;
  }
  LG("stu_srv_res_p.txt is opened!");

  int concurrent = atoi(argv[3]);
  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_addr.s_addr = inet_addr(argv[1]),
                                    .sin_port = htons(atoi(argv[2]))};

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  bind(listenfd, (struct sockaddr *)(&addr_server), sizeof(addr_server));
  listen(listenfd, BACKLOG);

  while (!sig_to_exit) {
    struct sockaddr_in addr_client;
    int addr_client_len = sizeof(addr_client);
    int connfd =
        accept(listenfd, (struct sockaddr *)(&addr_client), &addr_client_len);
    if (connfd == -1 && errno == EINTR) {
      if (sig_type == SIGINT) break;
      continue;
    }
    char ip[20];
    inet_ntop(AF_INET, &addr_client.sin_addr, ip, sizeof(ip));
    LOG(fp_res, "client[%s:%d] is accepted!", ip,
        (int)ntohs(addr_client.sin_port));
    if (!fork())
      process_socket(&addr_client, listenfd, connfd);
    else
      close(connfd);
  }

  close(listenfd);
  LOG(fp_res, "listenfd is closed!");
  LOG(fp_res, "parent process is going to exit!");
  fclose(fp_res);
  LG("stu_srv_res_p.txt is closed!");
  return 0;
}