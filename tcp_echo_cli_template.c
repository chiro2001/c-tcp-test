/*
	TCP Echo Standard Client Demo (Multiprocess Concurrency)
	Copyright@2021 张翔（zhangx@uestc.edu.cn）All Right Reserved
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CMD_STR 100
#define bprintf(fp, format, ...) \
	if(fp == NULL){printf(format, ##__VA_ARGS__);} 	\
	else{printf(format, ##__VA_ARGS__);	\
			fprintf(fp, format, ##__VA_ARGS__);fflush(fp);}

int sig_type = 0;
FILE * fp_res = NULL;//文件指针

void sig_pipe(int signo) {
	// TODO 记录本次系统信号编号到sig_type中;通过getpid()获取进程ID，按照指导书上的要求打印相关信息，并设置sig_to_exit的值


}


/*
业务函数，构造PDU，发送到服务器端，并接收回送
*/

int echo_rqt(int sockfd, int pin)
{
	pid_t pid = getpid();
	// PDU定义：PIN LEN Data
	int len_h = 0, len_n = 0;
	int pin_h = pin, pin_n = htonl(pin);
	char fn_td[10] = {0};
	char buf[MAX_CMD_STR+1+8] = {0}; //定义应用层PDU缓存
	int res=0;

	// 读取测试数据文件
	sprintf(fn_td, "td%d.txt", pin);
	FILE * fp_td = fopen(fn_td, "r");
	if(!fp_td){
		bprintf(fp_res, "[cli](%d) Test data read error!\n", pin_h);
		return 0;
	}

    // 读取一行测试数据，从编址buf+8的字节开始写入，前8个字节分别留给PIN与数据长度（均为int）
    while (fgets(buf+8, MAX_CMD_STR, fp_td)) {
	// 重置pin_h & pin_n:
		pin_h = pin;
		pin_n = htonl(pin);
	// 指令解析:
		// 收到指令"exit"，跳出循环并返回
		if(strncmp(buf+8, "exit", 4) == 0){
			// printf("[cli](%d) \"exit\" is found!\n", pin_h);
			break;
		}

	// 数据解析（构建应用层PDU）:
		// 将PIN写入PDU缓存（网络字节序）
		memcpy(buf, &pin_n, 4);
		// 获取数据长度
		len_h = strnlen(buf+8, MAX_CMD_STR);
		// 将数据长度写入PDU缓存（网络字节序）
		len_n = htonl(len_h);
		memcpy(buf+4, &len_n, 4);
		// TODO 将读入的'\n'更换为'\0'；若仅有'\n'输入，则'\0'将被作为数据内容发出，数据长度为1



	// 用write发送echo_rqt数据
        write(sockfd, buf, len_h+8);
    
	// 下面开始读取echo_rep返回来的数据，并存到res文件中
		memset(buf, 0, sizeof(buf));
		
		// 此部分的功能代码，建议参考服务器端echo_rep中的代码来编写，此处不再重复
		// TODO 读取PIN（网络字节序）到pin_n中
    	// TODO 读取服务器echo_rep数据长度（网络字节序）到len_n，并转为主机字节序存放到len_h
		// TODO读取服务器echo_rep数据，并输出到res文件中



















    }
	return 0;
}

int main(int argc, char* argv[])
{
	// 基于argc简单判断命令行指令输入是否正确；
	if(argc != 4){
		printf("Usage:%s <IP> <PORT> <CONCURRENT AMOUNT>\n", argv[0]);
		return 0;
	}

	struct sigaction sigact_pipe, old_sigact_pipe;
	sigact_pipe.sa_handler = sig_pipe;//sig_pipe()，信号处理函数
	sigemptyset(&sigact_pipe.sa_mask);
	sigact_pipe.sa_flags = 0;
	sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
	sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);

	// TODO 安装SIGCHLD信号处理器.这里可直接将handler设为SIG_IGN，忽略SIGCHLD信号即可，
	// 注意和上述SIGPIPE一样，也要设置受影响的慢系统调用重启。也可以按指导书说明用一个自定义的sig_chld
	// 函数来处理SIGCHLD信号(复杂些)






    //TODO 定义如下变量：
	// 服务器Socket地址srv_addr，客户端Socket地址cli_addr；
	// 客户端Socket地址长度cli_addr_len（类型为socklen_t）；
	// Socket连接描述符connfd；
	// 最大并发连接数（含父进程）conc_amnt,其值由命令行第三个参数决定（用atoi函数）






	// 获取当前进程PID，用于后续父进程信息打印；
	pid_t pid = getpid();

	//TODO 初始化服务器Socket地址srv_addr，其中会用到argv[1]、argv[2]
		/* IP地址转换推荐使用inet_pton()；端口地址转换推荐使用atoi(); */






	for (int i = 0; i < conc_amnt - 1; i++) {
        if (!fork()) {// 子进程
			int pin = i+1;
			char fn_res[20];	// 用于处理文件名的字符数组
			
			// TODO 获取当前子进程PID,用于后续子进程信息打印
			
			
			// 打开res文件，文件序号指定为当前子进程序号PIN；
			sprintf(fn_res, "stu_cli_res_%d.txt", pin);
        	fp_res = fopen(fn_res, "ab"); // Write only， append at the tail. Open or create a binary file;
			if(!fp_res){
				printf("[cli](%d) child exits, failed to open file \"stu_cli_res_%d.txt\"!\n", pid, pin);
				exit(-1);
			}

			// TODO 将子进程已创建的信息打印到stdout（格式见指导书）


			// TODO 创建套接字connfd（注意加上出错控制）
				




			do{
				int res;
				// 用connect连接到服务器端，返回值放在res里
				res = connect(connfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
				if(!res){
					char ip_str[20]={0};	//用于IP地址转换
					// TODO 将服务器端地址信息打印输出至对应的stu_cli_res_PIN.txt（见指导书）




					if(!echo_rqt(connfd, pin))	//调用业务处理函数echo_rqt
						break;
				}
				else
					break;	
			}while(1);

			// 关闭连接描述符
			close(connfd);
			bprintf(fp_res, "[cli](%d) connfd is closed!\n", pid);
			bprintf(fp_res, "[cli](%d) child process is going to exit!\n", pid);
			
			// TODO 关闭子进程res文件，同时打印提示信息到stdout(格式见指导书)




			exit(1);
		}
	}
	
// 下面在父进程中连接服务器端，操作和上述子进程中的代码类同
	char fn_res[20];
	sprintf(fn_res, "stu_cli_res_%d.txt", 0);
    fp_res = fopen(fn_res, "wb");
	if(!fp_res){
		printf("[cli](%d) child exits, failed to open file \"stu_cli_res_0.txt\"!\n", pid);
		exit(-1);
	}

	// TODO 创建套接字connfd（注意加上出错控制）
				




	do{
		int res;
		// 用connect连接到服务器端，返回值放在res里
		res = connect(connfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
		if(!res){
			char ip_str[20]={0};	//用于IP地址转换
			// TODO 将服务器端地址信息打印输出至对应的stu_cli_res_0.txt（见指导书）




			if(!echo_rqt(connfd, pin))	//调用业务处理函数echo_rqt
				break;
		}
		else
			break;	
	}while(1);

	// TODO 关闭连接描述符，
	

	bprintf(fp_res, "[cli](%d) connfd is closed!\n", pid);
	bprintf(fp_res, "[cli](%d) parent process is going to exit!\n", pid);

	// 关闭父进程res文件,并按指导书打印提示信息到stdout



	return 0;
}