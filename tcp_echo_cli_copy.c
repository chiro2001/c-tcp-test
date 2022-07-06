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

#define MAX_CMD_STR 100
// 后面的输出到文件操作，建议使用这个宏，还可同时在屏幕上显示出来
#define LOG(fp, format, ...)            \
  if (fp) {                             \
    printf(format, ##__VA_ARGS__);      \
    fprintf(fp, format, ##__VA_ARGS__); \
    fflush(fp);                         \
  } else {                              \
    printf(format, ##__VA_ARGS__);      \
  }

int sig_type = 0;
int sig_to_exit = 0;
FILE* fp_res = NULL;

int echo_rqt(int sockfd, int pin) {
  pid_t pid = getpid();
  int pin_h = pin, pin_n = htonl(pin);
  int len_h = 0, len_n = 0;
  char fn_td[10] = {0};
  char buf[MAX_CMD_STR + 1 + 8] = {0};
  int res = 0;

  sprintf(fn_td, "td%d.txt", pin);
  FILE* fp_td = fopen(fn_td, "r");
  if (!fp_td) {
    LOG(fp_res, "[cli](%d) Test data read error!\n", pin_h);
    return 0;
  }

  // 读取一行测试数据，从编址buf+8的字节开始写入，前8个字节分别留给PIN与数据长度（均为int）
  while (fgets(buf + 8, MAX_CMD_STR, fp_td)) {
    pin_h = pin;
    pin_n = htonl(pin);
    if (strncmp(buf + 8, "exit", 4) == 0) {
      break;
    }

    // 将PIN（网络字节序）写入PDU缓存
    memcpy(buf, &pin_n, 4);
    // 获取数据长度
    len_h = strnlen(buf + 8, MAX_CMD_STR);
    len_n = htonl(len_h);
    // 将数据长度写入PDU缓存（网络字节序）
    memcpy(buf + 4, &len_n, 4);

    // 将读入的'\n'更换为'\0'；若仅有'\n'输入，则'\0'将被作为数据内容发出，数据长度为1
    if (buf[len_h + 7] == '\n') buf[len_h + 7] = '\0';

    // 用write发送echo_rqt数据
    write(sockfd, buf, len_h + 8);

    memset(buf, 0, sizeof(buf));
    // 下面开始读取echo_rep返回来的数据，并存到res文件中
    // 读取PIN（网络字节序）到pin_n中
    res = read(sockfd, &pin_n, 4);
    // 读取服务器echo_rep数据长度（网络字节序）到len_n
    res = read(sockfd, &len_n, 4);
    len_h = ntohl(len_n);  //转为主机字节序存放到len_h
    int already_read = 0, remain_read = len_h;
    do {
      // 使用read读取客户端数据，返回值赋给res。
      res = read(sockfd, buf + already_read, remain_read);
      already_read += res;
      if (already_read < len_h) {
        remain_read = len_h - already_read;
      } else if (already_read == len_h) {
        break;
      } else {
        // free(buf);
        // do not need to free buf
        return pin_h;
      }
    } while (1);

    // 读取服务器echo_rep数据，并输出到res文件中
    LOG(fp_res, "[echo_rep](%d) %s\n", pid, buf);
  }

  return 0;
}
// 业务逻辑结束
void sig_pipe(int signo) {
  sig_type = signo;
  printf("[cli](%d) SIGINT is coming!\n", getpid());
  sig_to_exit = 1;
}
int main(int argc, char* argv[]) {
  if (argc != 4) {
    printf("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>\n", argv[0]);
    return 0;
  }
  struct sigaction sigactpipe, old_sigactpipe;
  sigactpipe.sa_handler = sig_pipe;
  sigemptyset(&sigactpipe.sa_mask);
  sigactpipe.sa_flags = 0;
  sigactpipe.sa_flags |= SA_RESTART;
  sigaction(SIGPIPE, &sigactpipe, &old_sigactpipe);

  struct sigaction sigact_chld, old_sigact_chld;
  sigact_chld.sa_handler = SIG_IGN;
  sigemptyset(&sigact_chld.sa_mask);
  sigact_chld.sa_flags = 0;
  sigact_chld.sa_flags |= SA_RESTART;
  sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);

  struct sockaddr_in srv_addr, cli_addr;
  socklen_t cli_addr_len;
  int connfd;                     //定义Socket连接描述符connfd；
  int conc_amnt = atoi(argv[3]);  // 最大并发连接数

  pid_t pid1 = getpid();

  srv_addr.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &srv_addr.sin_addr);
  srv_addr.sin_port = htons(atoi(argv[2]));

  for (int i = 0; i < conc_amnt - 1; i++) {
    if (!fork()) {  // 子进程
      int pin = i + 1;
      char fn_res[20];  // 用于处理文件名的字符数组

      pid_t pid2 = getpid();  // 获取当前子进程PID,用于后续子进程信息打印

      sprintf(fn_res, "stu_cli_res_%d.txt",
              pin);  // 打开res文件，文件序号指定为当前子进程序号PIN；
      fp_res = fopen(fn_res, "ab");
      if (!fp_res) {
        printf(
            "[cli](%d) child exits, failed to open file "
            "\"stu_cli_res_%d.txt\"!\n",
            pid2, pin);
        exit(-1);
      }

      printf("[cli](%d) stu_cli_res_%d.txt is created!\n", pid2,
             pin);  // 将子进程已创建的信息打印到stdout
      LOG(fp_res, "[cli](%d) child process %d is created!\n", pid2, pin);
      connfd =
          socket(AF_INET, SOCK_STREAM, 0);  // 创建套接字connfd（加上出错控制）
      if (connfd == -1 && errno == EINTR && sig_type == SIGINT) {
        break;
      }

      do {
        int res;
        res = connect(
            connfd, (struct sockaddr*)&srv_addr,
            sizeof(srv_addr));  // 用connect连接到服务器端，返回值放在res里
        if (!res) {
          char ip[20] = {0};  //用于IP地址转换
          LOG(fp_res, "[cli](%d) server[%s:%d] is connected!\n", pid2,
              inet_ntop(AF_INET, &srv_addr.sin_addr, ip, sizeof(ip)),
              // 将服务器端地址信息打印输出至对应的stu_cli_res_PIN.txt（见指导书）
              ntohs(srv_addr.sin_port));
          //调用业务处理函数echo_rqt
          if (!echo_rqt(connfd, pin)) break;
        } else
          break;
      } while (1);

      // 关闭连接描述符
      close(connfd);
      LOG(fp_res, "[cli](%d) connfd is closed!\n", pid2);
      LOG(fp_res, "[cli](%d) child process is going to exit!\n", pid2);

      fclose(fp_res);
      //关闭子进程res文件，同时打印提示信息到stdout
      printf("[cli](%d) stu_cli_res_%d.txt is closed!\n", pid2, pin);
      exit(1);
    }
  }
  char fn_res[20];
  sprintf(fn_res, "stu_cli_res_%d.txt", 0);
  fp_res = fopen(fn_res, "wb");
  if (!fp_res) {
    printf(
        "[cli](%d) child exits, failed to open file \"stu_cli_res_0.txt\"!\n",
        pid1);
    exit(-1);
  }

  connfd = socket(AF_INET, SOCK_STREAM, 0);  // 创建套接字connfd（加上出错控制）
  if (connfd == -1 && errno == EINTR && sig_type == SIGINT) {
    exit(-1);
  }

  int pin = 0;
  do {
    // 用connect连接到服务器端，返回值放在res里
    int res = connect(connfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if (!res) {
      char ip[20] = {0};  //用于IP地址转换
      LOG(fp_res, "[cli](%d) server[%s:%d] is connected!\n", pid1,
          inet_ntop(AF_INET, &srv_addr.sin_addr, ip, sizeof(ip)),
          // 将服务器端地址信息打印输出至对应的stu_cli_res_PIN.txt（见指导书）
          ntohs(srv_addr.sin_port));

      // 调用业务处理函数echo_rqt
      if (!echo_rqt(connfd, pin)) break;
    } else
      break;
  } while (1);

  // 关闭连接描述符，
  close(connfd);
  LOG(fp_res, "[cli](%d) connfd is closed!\n", pid1);
  LOG(fp_res, "[cli](%d) parent process is going to exit!\n", pid1);

  fclose(fp_res);
  //关闭子进程res文件，同时打印提示信息到stdout
  printf("[cli](%d) stu_cli_res_%d.txt is closed!\n", pid1, pin);

  return 0;
}
