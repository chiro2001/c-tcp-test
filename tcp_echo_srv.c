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

#define LOG_SELF "srv"
#define LOG_PREFIX "[%s](%d) "

#define LOG_(fp, name, format, ...)                                       \
  do {                                                                    \
    printf(LOG_PREFIX format "\n", name, (int)(getpid()), ##__VA_ARGS__); \
    if (fp) {                                                             \
      fprintf(fp, LOG_PREFIX format "\n", name, (int)(getpid()),          \
              ##__VA_ARGS__);                                             \
      fflush(fp);                                                         \
    }                                                                     \
  } while (0);

#define LOG(fp, format, ...) LOG_(fp, LOG_SELF, format, ##__VA_ARGS__)

#define LOG_RQT(fp, format, ...) LOG_(fp, "echo_rqt", format, ##__VA_ARGS__)
#define LOG_REP(fp, format, ...) LOG_(fp, "echo_rep", format, ##__VA_ARGS__)

#define LG(format, ...) LOG(NULL, format, ##__VA_ARGS__)

void setup_signal_handler(int signal_number, void (*function)(int), int flags) {
  struct sigaction sig;
  sig.sa_flags = flags, sig.sa_handler = function;
  sigemptyset(&sig.sa_mask);
  sigaction(signal_number, &sig, NULL);
}

int sig_to_exit = 0;
int sig_type = 0;
FILE *fp_res = NULL;

void handle_signal(int signo) {
  sig_type = signo;
  sig_to_exit = 1;
  LOG(fp_res, "%s is coming!", (signo != SIGINT ? "SIGPIPE" : "SIGINT"));
}

uint8_t *writeto(void *dst, void *src, uint32_t size) {
  memcpy(dst, src, size);
  return (uint8_t *)(dst) + size;
}

int echo_req(int sockfd) {
  uint32_t pin_n = -1;
  uint32_t len_n = -1;
  int pin = -1;
  int len = -1;
  int res = 0;
  do {
    do {
      res = read(sockfd, &pin_n, 4);
      if (!res) return pin;
      pin = ntohl(pin_n);
      break;
    } while (1);
    do {
      res = read(sockfd, &len_n, 4);
      if (!res) return len;
      len = ntohl(len_n);
      break;
    } while (1);
    uint8_t *buf_read = malloc(len * sizeof(uint8_t));
    uint8_t *buf = malloc(len * sizeof(uint8_t) + 9);
#ifndef QRT_DO_EXIT
#define QRT_DO_EXIT \
  do {              \
    free(buf);      \
    free(buf_read); \
    return pin;     \
  } while (0)
#endif
    int read_bytes = 0;
    do {
      res = read(sockfd, buf_read + read_bytes, len - read_bytes);
      if (res == 0) QRT_DO_EXIT;
      if (read_bytes + res > len) QRT_DO_EXIT;
      read_bytes += res;
      if (read_bytes == len) break;
    } while (1);
    buf_read[read_bytes] = '\0';
    LOG_RQT(fp_res, "%s", buf_read);
    writeto(writeto(writeto(buf, &pin_n, 4), &len_n, 4), buf_read, len);
    write(sockfd, buf, len + 8);
    free(buf);
    free(buf_read);
  } while (1);
  return pin;
}

void process_socket(int listenfd, int connfd) {
  char filename[32];
  sprintf(filename, "stu_srv_res_%d.txt", getpid());
  fp_res = fopen(filename, "wb");
  if (!fp_res) {
    LG("child exits, failed to open file \"stu_cli_res_%d.txt\"!", getpid());
    exit(-1);
  }
  LOG(fp_res, "child process is created!");

  close(listenfd);
  LOG(fp_res, "listenfd is closed!");

  int pin = echo_req(connfd);
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
  fclose(fp_res);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    LG("Usage:%s <IP> <PORT>", argv[0]);
    return 0;
  }
  setup_signal_handler(SIGPIPE, handle_signal, SA_RESTART);
  setup_signal_handler(SIGCHLD, SIG_IGN, SA_RESTART);
  setup_signal_handler(SIGINT, handle_signal, 0);

  fp_res = fopen("stu_srv_res_p.txt", "wb");
  if (!fp_res) {
    return 1;
  }

  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_addr.s_addr = inet_addr(argv[1]),
                                    .sin_port = htons(atoi(argv[2]))};

  char ip[20];
  inet_ntop(AF_INET, &addr_server.sin_addr, ip, sizeof(ip));
  LOG(fp_res, "server[%s:%d] is initializing!", ip,
      (int)ntohs(addr_server.sin_port));
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
      process_socket(listenfd, connfd);
    else
      close(connfd);
  }

  close(listenfd);
  LOG(fp_res, "listenfd is closed!");
  LOG(fp_res, "parent process is going to exit!");
  fclose(fp_res);
  return 0;
}