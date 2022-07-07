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

#define LOG_SELF "cli"
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

#define MAX_CMD_STR 100

int sig_type = 0;
int sig_to_exit = 0;
FILE *fp_res = NULL;

void sig_int(int signo) {
  sig_type = signo;
  sig_to_exit = 1;
  LG("SIGINT is coming!");
  exit(1);
}

uint8_t *writeto(uint8_t *dst, uint8_t *src, uint32_t size) {
  uint32_t i = size;
  uint8_t *d = (uint8_t *)dst;
  while (i--) {
    *(d++) = *(src++);
  }
  return (uint8_t *)(dst) + size;
}

int echo_rqt(int sockfd, int pin) {
  char filename_td[20];
  sprintf(filename_td, "td%d.txt", pin);
  FILE *fp_td = fopen(filename_td, "r");
  if (!fp_td) {
    LOG(fp_res, "Test data read error!");
    return 0;
  }
  uint8_t buf[9 + MAX_CMD_STR];
  char buf_read[1 + MAX_CMD_STR];
  memset(buf, 0, sizeof(buf));
  memset(buf_read, 0, sizeof(buf_read));
  while (fgets(buf_read, MAX_CMD_STR, fp_td)) {
    if (strncmp(buf_read, "exit", 4) == 0) {
      break;
    }
    if (buf_read[0] == '\n') buf_read[0] = '\0';
    int len = strnlen(buf_read, MAX_CMD_STR);
    len -= (buf_read[len - 1] == '\n');
    buf_read[len - 1] = (buf_read[len - 1] == '\n' ? '\0' : buf_read[len - 1]);
    uint32_t len_n = htonl(len ? len : 1);
    uint8_t *p = buf;
    uint32_t pin_n = htonl(pin);
    writeto(writeto(writeto(buf, (uint8_t *)&pin_n, 4), (uint8_t *)&len_n, 4),
            buf_read, len + 1);
    write(sockfd, buf, len + 8);
    memset(buf, 0, sizeof(buf));
    read(sockfd, &pin_n, 4);
    read(sockfd, &len_n, 4);
    len = ntohl(len_n);
    int read_bytes = 0;
    int remain_bytes = len;
    do {
      ssize_t res = read(sockfd, buf + read_bytes, remain_bytes);
      if (res < 0) {
        return 0;
      }
      if (res == 0 && sig_to_exit) {
        return 0;
      }
      read_bytes += res;
      if (read_bytes == len)
        break;
      else if (read_bytes > len) {
        return pin;
      } else {
        remain_bytes = len - read_bytes;
      }
    } while (1);
    LOG_REP(fp_res, "%s", buf);
  }
  return 0;
}

void process_socket(struct sockaddr_in *addr_server, int pin) {
  char filename[32];
  sprintf(filename, "stu_cli_res_%d.txt", pin);
  fp_res = fopen(filename, "ab");
  if (!fp_res) {
    LG("child exits, failed to open file \"stu_cli_res_%d.txt\"!", pin);
    exit(-1);
  }
  LOG(fp_res, "child process %d is created!", pin);
  int connfd = socket(AF_INET, SOCK_STREAM, 0);
  if (connfd == -1 && errno == EINTR && sig_type == SIGINT) {
    exit(-1);
  }
  do {
    if (!connect(connfd, (struct sockaddr *)addr_server,
                 sizeof(struct sockaddr))) {
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
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 4) {
    LG("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>", argv[0]);
    return 0;
  }
  setup_signal_handler(SIGINT, sig_int, SA_RESTART);
  setup_signal_handler(SIGPIPE, sig_int, SA_RESTART);
  setup_signal_handler(SIGCHLD, SIG_IGN, SA_RESTART);

  int concurrent = atoi(argv[3]);
  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_port = htons(atoi(argv[2]))};
  inet_pton(AF_INET, argv[1], &addr_server.sin_addr);

  for (int pin = 1; pin < concurrent; pin++)
    if (!fork()) process_socket(&addr_server, pin);
  process_socket(&addr_server, 0);
}