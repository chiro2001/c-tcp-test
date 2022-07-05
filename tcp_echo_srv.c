#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <assert.h>

#define BACKLOG 1024
#define myprintf(fp, format, ...) \
	if(fp == NULL){printf(format, ##__VA_ARGS__);} 	\
	else{printf(format, ##__VA_ARGS__);	\
			fprintf(fp, format, ##__VA_ARGS__);fflush(fp);}	//后面的输出到文件操作，建议使用这个宏，还可同时在屏幕上显示出来

int sig_type = 0, sig_to_exit = 0;
FILE * fp_res = NULL;	//文件指针

void sig_int(int signo)
{
	sig_type=signo;
    myprintf(fp_res,"[srv](%d) SIGINT is coming!\n",getpid());
    sig_to_exit=1;
}
void sig_pipe(int signo)
{
	sig_type=signo;
    myprintf(fp_res,"[srv](%d) SIGPIPE is coming!\n",getpid());
    sig_to_exit=1;
}
/*
int install_sig_handlers()
功能：安装SIGPIPE,SIGCHLD,SIGINT三个信号的处理函数
	返回值：
		-1，安装SIGPIPE失败；
		-2，安装SIGCHLD失败；
		-3，安装SIGINT失败；
		0，都成功
*/
int install_sig_handlers()
{
	int res = -1;
	struct sigaction sigact_pipe, old_sigact_pipe;
	sigact_pipe.sa_handler = sig_pipe;//sig_pipe()，信号处理函数
	sigact_pipe.sa_flags = 0;
	sigact_pipe.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
	sigemptyset(&sigact_pipe.sa_mask);
	res = sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);
	if(res)
	    return -1;
    struct sigaction sigact_chld, old_sigact_chld;
    sigact_chld.sa_handler=SIG_IGN;
    sigemptyset(&sigact_chld.sa_mask);
    sigact_chld.sa_flags = 0;
    sigact_chld.sa_flags |= SA_RESTART;//设置受影响的慢系统调用重启
    res = sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);
    if(res)
        return -2;
	// TODO 安装SIGINT信号处理器,若失败返回-3
    struct sigaction sigact_int, old_sigact_int;
    sigact_int.sa_handler = sig_int;
    sigact_int.sa_flags = 0;
    sigemptyset(&sigact_int.sa_mask);
    res=sigaction(SIGINT, &sigact_int, &old_sigact_int);
    if(res)
        return -3;
	return 0;
}


int echo_rep(int sockfd)
{
	int len_h;
	int len_n;	
	int pin_h;
	int pin_n;
	int res = 0;
	char *buf = NULL;
	pid_t pid = getpid();
    do {
		do{
            res=read(sockfd,&pin_n,4);
			if(res < 0)
			{
				myprintf(fp_res, "[srv](%d) read pin_n return %d and errno is %d!\n", pid, res, errno);
				if(errno == EINTR)
				{
					if(sig_type == SIGINT)
						return pin_h;
					continue;
				}
				return pin_h;
			}
			if(!res)
			{
				return pin_h;
			}
            pin_h=ntohl(pin_n);
			break;				
		}while(1);

		// 读取客户端echo_rqt数据长度
		do{
			res=read(sockfd,&len_n,4);
			if(res < 0)
			{
				myprintf(fp_res, "[srv](%d) read len_n return %d and errno is %d\n", pid, res, errno);
				if(errno == EINTR)
				{
					if(sig_type == SIGINT)
						return len_h;
					continue;
				}
				return len_h;
			}
			if(!res)
			{
				return len_h;
			}
            len_h=ntohl(len_n);
			break;
		}while(1);
	
		buf = (char*)malloc(len_h * sizeof(char)+8); // 预留PID与数据长度的存储空间，为后续回传做准备
		
		int already_read=0;
		int remain_read=len_h;
		do{
			//TODO 使用read读取客户端数据，返回值赋给res。注意read第2、3个参数，即每次存放的缓冲区的首地址及所需读取的长度如何设定
			res=read(sockfd,buf+already_read+8,remain_read);
			if(res < 0){
				myprintf(fp_res, "[srv](%d) read data return %d and errno is %d,\n", pid, res, errno);
				if(errno == EINTR)
				{
					if(sig_type == SIGINT)
					{
						free(buf);
						return pin_h;
					}
					continue;
				}
				free(buf);
				return pin_h;
			}
			if(!res)
			{
				free(buf);
				return pin_h;
			}
			//TODO 此处计算read_amnt及len_to_read的值，注意考虑已读完和未读完两种情况，以及其它情况（此时直接用上面的代码操作，free(buf),并 return pin_h）
			already_read+=res;
            if(already_read==len_h)
            {
               break; 
            }
            else if (already_read<len_h)
            {
              remain_read=len_h-already_read;
            }
            else{
                free(buf);
                return pin_h;
            }
		}while(1);
        myprintf(fp_res,"[echo_rqt](%d) %s\n",pid,buf+8);
		memcpy(buf, &pin_n, 4);
		memcpy(buf+4, &len_n, 4);
        write(sockfd,buf,len_h+8);
        free(buf);
    }while(1);
	return pin_h;
}

int main(int argc, char* argv[])
{
	// 基于argc简单判断命令行指令输入是否正确；
	if(argc != 3)
	{
		printf("Usage:%s <IP> <PORT>\n", argv[0]);
		return -1;
	}

	pid_t pid = getpid();
	char ip[20]={0};
	char fn_res[20]={0};
	// 定义结果变量，用于承接后续系统调用返回结果；
	int res = -1;
    res=install_sig_handlers();
    if(res!=0)
    {
        return res;
    }
	fp_res = fopen("stu_srv_res_p.txt", "wb");
	if(!fp_res)
	{
		printf("[srv](%d) failed to open file \"stu_srv_res_p.txt\"!\n", pid);
		return res;
	}
	printf("[srv](%d) stu_srv_res_p.txt is opened!\n",pid);

    struct sockaddr_in srv_addr, cli_addr;
    socklen_t cli_addr_len = sizeof(struct sockaddr_in);
	
	int listenfd,connfd;


    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    srv_addr.sin_port = htons(atoi(argv[2]));



    myprintf(fp_res,"[srv](%d) server[%s:%d] is initializing!\n",pid,inet_ntop(AF_INET,&srv_addr.sin_addr,ip,sizeof(ip)),(int)ntohs(srv_addr.sin_port));

	
    listenfd=socket(AF_INET, SOCK_STREAM, 0);


    res=bind(listenfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));



    res=listen(listenfd,BACKLOG);



	
    // 开启accpet()主循环，直至sig_to_exit指示服务器退出；
	while(!sig_to_exit)
	{
		
        cli_addr_len=sizeof(cli_addr);
        connfd=accept(listenfd,(struct sockaddr*)&cli_addr,&cli_addr_len);
	
	
		// 以下代码紧跟accept()，用于判断accpet()是否因SIG_INT信号退出（本案例中只关心SIGINT）；也可以不做此判断，直接执行 connfd<0 时continue，因为此时sig_to_exit已经指明需要退出accept()主循环，两种方式二选一即可。
		if(connfd == -1 && errno == EINTR)
		{
			if(sig_type == SIGINT)
				break;
			continue;
		}
	
        myprintf(fp_res,"[srv](%d) client[%s:%d] is accepted!\n",pid,inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip)), (int)ntohs(cli_addr.sin_port));


		// 派生子进程对接客户端开展业务交互
		if(!fork())
		{// 子进程
	
			// 获取当前子进程PID,用于后续子进程信息打印
			pid_t pid2 = getpid();
			// 打开res文件，首先基于PID命名，随后在子进程退出前再根据echo_rep()返回的PIN值对文件更名；
			sprintf(fn_res, "stu_srv_res_%d.txt", pid2);
			fp_res = fopen(fn_res, "wb");// Write only， append at the tail. Open or create a binary file;
			if(!fp_res)
			{
				printf("[srv](%d) child exits, failed to open file \"stu_srv_res_%d.txt\"!\n", pid2, pid2);
				exit(-1);
			}
			
            printf("[srv](%d) stu_srv_res_%d.txt is opened!\n",pid2,pid2);

			
            close(listenfd);
            myprintf(fp_res,"[srv](%d) child process is created!\n",pid2);
			myprintf(fp_res,"[srv](%d) listenfd is closed!\n",pid2);
			
			int pin;
			pin=echo_rep(connfd);
			if(pin < 0)
			{
				myprintf(fp_res, "[srv](%d) child exits, client PIN returned by echo_rqt() error!\n", pid2);
				exit(-1);
			}

		
			char fn_res_n[20]={0};
			sprintf(fn_res_n, "stu_srv_res_%d.txt", pin);
			if(!rename(fn_res, fn_res_n))
			{
				myprintf(fp_res, "[srv](%d) res file rename done!\n", pid2);
			}
			else{
				myprintf(fp_res, "[srv](%d) child exits, res file rename failed!\n", pid2);
			}

		
			close(connfd);
            myprintf(fp_res, "[srv](%d) connfd is closed!\n", pid2);
            myprintf(fp_res, "[srv](%d) child process is going to exit!\n", pid2);
			
            fclose(fp_res);
            printf("[srv](%d) stu_srv_res_%d.txt is closed!\n",pid2,pid2);
            exit(1);
		}
		else{
		    close(connfd);
		}
	}

	close(listenfd);
	myprintf(fp_res, "[srv](%d) listenfd is closed!\n", pid);
	myprintf(fp_res, "[srv](%d) parent process is going to exit!\n", pid);

    fclose(fp_res);
    printf("[srv](%d) stu_srv_res_p.txt is closed!\n",pid);
	return 0;
}

