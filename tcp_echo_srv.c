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

// macro stringizing
#define str_temp(x) #x
#define str(x) str_temp(x)

// strlen() for string constant
#define STRLEN(CONST_STR) (sizeof(CONST_STR) - 1)

// calculate the length of an array
#define ARRLEN(arr) (int)(sizeof(arr) / sizeof(arr[0]))

// macro concatenation
#define concat_temp(x, y) x##y
#define concat(x, y) concat_temp(x, y)
#define concat3(x, y, z) concat(concat(x, y), z)
#define concat4(x, y, z, w) concat3(concat(x, y), z, w)
#define concat5(x, y, z, v, w) concat4(concat(x, y), z, v, w)

// macro testing
// See
// https://stackoverflow.com/questions/26099745/test-if-preprocessor-symbol-is-defined-inside-macro
#define CHOOSE2nd(a, b, ...) b
#define MUX_WITH_COMMA(contain_comma, a, b) CHOOSE2nd(contain_comma a, b)
#define MUX_MACRO_PROPERTY(p, macro, a, b) \
  MUX_WITH_COMMA(concat(p, macro), a, b)
// define placeholders for some property
#define __P_DEF_0 X,
#define __P_DEF_1 X,
#define __P_ONE_1 X,
#define __P_ZERO_0 X,
// define some selection functions based on the properties of BOOLEAN macro
#define MUXDEF(macro, X, Y) MUX_MACRO_PROPERTY(__P_DEF_, macro, X, Y)
#define MUXNDEF(macro, X, Y) MUX_MACRO_PROPERTY(__P_DEF_, macro, Y, X)
#define MUXONE(macro, X, Y) MUX_MACRO_PROPERTY(__P_ONE_, macro, X, Y)
#define MUXZERO(macro, X, Y) MUX_MACRO_PROPERTY(__P_ZERO_, macro, X, Y)

// test if a boolean macro is defined
#define ISDEF(macro) MUXDEF(macro, 1, 0)
// test if a boolean macro is undefined
#define ISNDEF(macro) MUXNDEF(macro, 1, 0)
// test if a boolean macro is defined to 1
#define ISONE(macro) MUXONE(macro, 1, 0)
// test if a boolean macro is defined to 0
#define ISZERO(macro) MUXZERO(macro, 1, 0)
// test if a macro of ANY type is defined
// NOTE1: it ONLY works inside a function, since it calls `strcmp()`
// NOTE2: macros defined to themselves (#define A A) will get wrong results
#define isdef(macro) (strcmp("" #macro, "" str(macro)) != 0)

// simplification for conditional compilation
#define __IGNORE(...)
#define __KEEP(...) __VA_ARGS__
// keep the code if a boolean macro is defined
#define IFDEF(macro, ...) MUXDEF(macro, __KEEP, __IGNORE)(__VA_ARGS__)
// keep the code if a boolean macro is undefined
#define IFNDEF(macro, ...) MUXNDEF(macro, __KEEP, __IGNORE)(__VA_ARGS__)
// keep the code if a boolean macro is defined to 1
#define IFONE(macro, ...) MUXONE(macro, __KEEP, __IGNORE)(__VA_ARGS__)
// keep the code if a boolean macro is defined to 0
#define IFZERO(macro, ...) MUXZERO(macro, __KEEP, __IGNORE)(__VA_ARGS__)

// functional-programming-like macro (X-macro)
// apply the function `f` to each element in the contain `c`
// NOTE1: `c` should be defined as a list like:
//   f(a0) f(a1) f(a2) ...
// NOTE2: each element in the contain can be a tuple
#define MAP(c, f) c(f)

#define BITMASK(bits) ((1 << (bits)) - 1)
#define BITS(x, hi, lo) \
  (((x) >> (lo)) & BITMASK((hi) - (lo) + 1))  // similar to x[hi:lo] in verilog
#define SEXT(x, len)   \
  ({                   \
    struct {           \
      int64_t n : len; \
    } __x = {.n = x};  \
    (int64_t) __x.n;   \
  })

#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz)-1) & ~((sz)-1))
#define ROUNDDOWN(a, sz) ((((uintptr_t)a)) & ~((sz)-1))

#define PG_ALIGN __attribute((aligned(4096)))

#if 1
#define likely(cond) __builtin_expect(cond, 1)
#define unlikely(cond) __builtin_expect(cond, 0)
#else
#define likely(cond) (cond)
#define unlikely(cond) (cond)
#endif

#define LOG_SELF MUXDEF(SELF_CLI, "cli", "srv")
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
  struct sigaction sig = {.sa_flags = flags, .__sigaction_handler = function};
  sigemptyset(&sig.sa_mask);
  sigaction(signal_number, &sig, NULL);
}

int sig_to_exit = 0;
int sig_type = 0;
FILE *fp_res = NULL;

void handle_signal(int signo, const char *type) {
  sig_type = signo;
  LOG(fp_res, "%s is coming!", type);
  sig_to_exit = 1;
}

void sig_int(int signo) { handle_signal(signo, "SIGINT"); }
void sig_pipe(int signo) { handle_signal(signo, "SIGPIPE"); }

uint8_t *write_bytes(void *dst, void *src, uint32_t size) {
  memcpy(dst, src, size);
  return (uint8_t *)(dst) + size;
}

int echo_req(int sockfd) {
  uint32_t pin_n = -1;
  uint32_t len_n = -1;
  int pin = -1;
  int len = -1;
  int res = 0;
  LG("\t* echo_req");
  do {
    do {
      res = read(sockfd, &pin_n, 4);
      LG("\t* server echo_req read pin: %d", res);
      if (res < 0) {
        LOG(fp_res, "read pin_n return %d and errno is %d!", res, errno);
        if (errno == EINTR) {
          if (sig_type == SIGINT) return pin;
          continue;
        }
        return pin;
      }
      if (!res) return pin;
      pin = ntohl(pin_n);
      break;
    } while (1);
    LG("* got pin: %d", pin);
    do {
      res = read(sockfd, &len_n, 4);
      if (res < 0) {
        LOG(fp_res, "read len_n return %d and errno is %d!", res, errno);
        if (errno == EINTR) {
          if (sig_type == SIGINT) return len;
          continue;
        }
        return len;
      }
      if (!res) return len;
      len = ntohl(len_n);
      break;
    } while (1);
    uint8_t *buf_read = malloc(len * sizeof(uint8_t));
    uint8_t *buf = malloc(len * sizeof(uint8_t) + 9);
#define QRT_DO_EXIT \
  do {              \
    free(buf);      \
    free(buf_read); \
    return pin;     \
  } while (0)
    int read_bytes = 0;
    do {
      res = read(sockfd, buf_read + read_bytes, len - read_bytes);
      if (res < 0) {
        LOG(fp_res, "read data return %d and errno is %d", res, errno);
        if (errno == EINTR) {
          if (sig_type == SIGINT) {
            QRT_DO_EXIT;
          }
          continue;
        }
        QRT_DO_EXIT;
      }
      if (res == 0) {
        QRT_DO_EXIT;
      }
      read_bytes += res;
      if (read_bytes > len) {
        QRT_DO_EXIT;
      }
      if (read_bytes == res) break;
    } while (1);
    buf_read[read_bytes] = '\0';
    LG("\t* read_bytes = %d", read_bytes);
    LOG_RQT(fp_res, "%s", buf_read);
    uint8_t *p = buf;
    p = write_bytes(p, &pin_n, 4);
    p = write_bytes(p, &len_n, 4);
    p = write_bytes(p, buf_read, len);
    write(sockfd, buf, len + 8);
    free(buf);
    free(buf_read);
  } while (1);
  return pin;
}

void process_socket(struct sockaddr_in *addr_client, int listenfd, int connfd) {
  LG("\t* process_socket");
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

  int pin = echo_req(connfd);
  if (pin < 0) {
    LOG(fp_res, "child exits, client PIN returned by echo_rqt() error!");
    LG("* pin = %d", pin);
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
  setup_signal_handler(SIGPIPE, sig_pipe, SA_RESTART);
  setup_signal_handler(SIGCHLD, SIG_IGN, SA_RESTART);
  setup_signal_handler(SIGINT, sig_int, 0);

  fp_res = fopen("stu_srv_res_p.txt", "wb");
  if (!fp_res) {
    LG("failed to open file \"stu_srv_res_p.txt\"!");
    return 1;
  }
  LG("stu_srv_res_p.txt is opened!");

  // 解析地址参数
  struct sockaddr_in addr_server = {.sin_family = AF_INET,
                                    .sin_addr.s_addr = inet_addr(argv[1]),
                                    .sin_port = htons(atoi(argv[2]))};

  char ip[20];
  inet_ntop(AF_INET, &addr_server.sin_addr, ip, sizeof(ip));
  LOG(fp_res, "server[%s:%d] is initializing!", ip, (int)ntohs(addr_server.sin_port));

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  bind(listenfd, (struct sockaddr *)(&addr_server), sizeof(addr_server));

  LG("* listening...");
  listen(listenfd, BACKLOG);

  while (!sig_to_exit) {
    struct sockaddr_in addr_client;
    int addr_client_len = sizeof(addr_client);
    LG("* accepting...");
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