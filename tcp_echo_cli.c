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
  struct sigaction sig = {.sa_flags = flags, .sa_handler = function};
  struct sigaction old;
  sigemptyset(&sig.sa_mask);
  sigaction(signal_number, &sig, &old);
}

#define MAX_CMD_STR 100

int sig_type = 0;
int sig_to_exit = 0;
FILE *fp_res = NULL;

void sig_int(int signo) {
  sig_type = signo;
  sig_to_exit = 1;
  LG("SIGINT is coming!");
}

uint8_t *write_bytes(void *dst, void *src, uint32_t size) {
  memcpy(dst, src, size);
  return (uint8_t *)(dst) + size;
}

int echo_rqt(int sockfd, int pin) {
  LG("* echo_rqt");
  uint32_t pin_n = htonl(pin);
  char filename_td[20];
  sprintf(filename_td, "td%d.txt", pin);
  FILE *fp_td = fopen(filename_td, "r");
  if (!fp_td) {
    LOG(fp_res, "Test data read error!");
    return 0;
  } else
    LG("* test data opened done.");
  uint8_t buf[10 + MAX_CMD_STR];
  char buf_read[1 + MAX_CMD_STR];
  memset(buf, 0, sizeof(buf));
  while (fgets(buf_read, MAX_CMD_STR, fp_td)) {
    if (buf_read[0] == '\n') buf_read[0] = '\0';
    LG("* read test data: %s", buf_read);
    if (strncmp(buf_read, "exit", 4) == 0) {
      LG("* client exit caused by data: %s", buf_read);
      break;
    }
    int len = strnlen(buf_read, MAX_CMD_STR);
    if (buf_read[len - 1] == '\n') {
      buf_read[len - 1] = '\0';
      len--;
    }
    int len_n = htonl(len ? len : 1);
    uint8_t *p = buf;
    p = write_bytes(p, &pin_n, 4);
    p = write_bytes(p, &len_n, 4);
    p = write_bytes(p, buf_read, len + 1);
    LG("* clien writing: %s", buf_read);
    write(sockfd, buf, len + 8);
    // refill
    memset(buf, 0, sizeof(buf));
    read(sockfd, &pin_n, 4);
    read(sockfd, &len_n, 4);
    len = ntohl(len_n);
    int read_bytes = 0;
    do {
      LG("** before read");
      ssize_t res = read(sockfd, buf + read_bytes, len - read_bytes);
      if (res < 0) {
        return 0;
      }
      if (res == 0 && sig_to_exit) {
        LG("** sig_to_exit");
        return 0;
      }
      read_bytes += res;
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
    exit(-1);
  }
  LG("stu_cli_res_%d.txt is created!", pin);
  LOG(fp_res, "child process %d is created!", pin);
  int connfd = socket(AF_INET, SOCK_STREAM, 0);
  if (connfd == -1 && errno == EINTR && sig_type == SIGINT) {
    // return 0;
    exit(-1);
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