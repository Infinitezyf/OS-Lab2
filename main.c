#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define POOL_SZ 10          //The size of the cmdPool 命令池大小
#define CMD_LENG 8          //The max_length of a simple cmd  = 参数 + 1 
#define PARA_MAX 64         //The max_length of a parameter 参数长度
#define LINE_MAX 5120       //The max length of a line from input
#define SHM_MEM_SIZE 10240  //The size of share memory


//a simple comand structure
typedef struct CMD_STRUCT
{
	char *cmd[CMD_LENG];
	char cmdStr[CMD_LENG*PARA_MAX];
	char nextSign; // '|' or'>' or '<' 
}cmdStruct;

//command pool including N simple command and pointer
typedef struct CMD_POOL_STRUCT
{
	cmdStruct cmdPool[POOL_SZ];
	int cmdPtr;//指针指向当前命令
}cmdPoolStruct;

extern cmdPoolStruct *pool;


//当前目录 全局变量  current directory of Shell
char dirPromt[20];

//list of builtin commands 内部变量
char *builtinStr[]={"cd","help","exit"};

int shm_id;
char *shm_buff;


//初始化共享内存
int sp_shm_init(void)
{
	int key;

	key = ftok("StupidShell",0);
	shm_id = shmget(key,SHM_MEM_SIZE,IPC_CREAT|0666);//IPC_CREAT新建 权限0666
	shm_buff = (char *)shmat(shm_id,NULL,0);
	pool = (cmdPoolStruct *)shm_buff;
	
	return 0;
}

//复制一段字符串
int sp_subString(char *resultStr, char *str , int origin , int end)
{
	int i,j=0;
	for(i=origin;i<=end;i++)
	{
		resultStr[j++]=str[i];
	}
	resultStr[j]='\0';

	return 0;
}

//根据指定字符串分割
int sp_splitStr(char *resultArr[], char *str , char *split)
{
	int i,position = 0;
	char *token;

	token = strtok(str, split);
	while (token != NULL)
	{
		resultArr[position]=token;

		position++;
		token = strtok(NULL, split);
	}
	resultArr[position] = NULL;

	return 0;
}

//初始化提示符
int sp_init()
{
	char *dirArr[10];
	char dir[200];
	int i;

	pool->cmdPtr=0;
	//getcwd()会将当前工作目录的绝对路径复制到参数buffer所指的内存空间中,参数maxlen为buffer的空间大小
	getcwd(dir,200);

	sp_splitStr(dirArr,dir,"/");

	//find current directory string
	for(i=0; ;i++)
	{
		if(NULL == dirArr[i])
			break;
	}

	strcpy( dirPromt , dirArr[i-1] );

	return 0;
}

//内部命令1——cd 改变当前目录
int sp_cd(void)
{
	int i;
	char *dirArr[10];
	char dir[200];
	
	if( 0 != chdir(pool->cmdPool[0].cmd[1]) )
	{
		perror("chdir");
		return -1;
	}
	else
	{
		getcwd(dir,200);//将当前工作目录复制到buffer
		sp_splitStr(dirArr,dir,"/");//分割存储到dirArr

		//find current directory string
		for(i=0; ;i++)
		{
			if(NULL == dirArr[i])
				break;
		}

		strcpy( dirPromt , dirArr[i-1] );//

		return 0;
	}
	
}


//内部命令2——help
int sp_help(void)
{
	printf("This is ZYF's Shell~\n");
	printf("What can I do for you?\n");
	printf("Type 'exit' to exit .\n");
	return 0;
}


//内部命令3——退出
int sp_exit(void)
{
	printf("exiting...\n");

	shmdt(shm_buff);
	shmctl(shm_id,IPC_RMID,0);

	exit(EXIT_SUCCESS);
	return 0;
}


//提示符样式
int sp_prompt()
{
	printf("\033[0;33m%s\033[0m",dirPromt);
	printf("\033[0;31m$\033[0m");
	return 0;
}


//brief read a line from standard input
int sp_readLine(char * line)
{
	int position = 0;
	char c;

	while (1)
	{
		c = getchar();

		if (c == '\n')
		{
			line[position] = '\n';
			break;
		}
		else
		{
			line[position] = c;
		}
		position++;
	}

	return 0;
}


//analyse and format the command
int sp_analyCmd(char * line )
{
	int i,j=0;
	int lastPosition=0;

	for(i=0; '\n'!=line[i] ;i++)
	{
		if( '|'==line[i] || '<'==line[i] || '>'==line[i] )//重定向 管道
		{
			sp_subString(pool->cmdPool[j].cmdStr,line,lastPosition,i-1);
			sp_splitStr(pool->cmdPool[j].cmd , pool->cmdPool[j].cmdStr, " ");
			pool->cmdPool[j].nextSign=line[i];//nextSign= | < >
			lastPosition=i+1;
			j++;
		}
	}
	//非重定向 管道的情况
	sp_subString(pool->cmdPool[j].cmdStr,line,lastPosition,i-1);
	sp_splitStr(pool->cmdPool[j].cmd , pool->cmdPool[j].cmdStr, " ");

	return 0;
}


//执行内部命令
int sp_builtinCmd(void)
{
	int i;
	
	for(i=0;i<3;i++)
	{
		if(0==strcmp(builtinStr[i],pool->cmdPool[0].cmd[0]) )
			break;
	}

	switch(i)
	{
	case 0:
		sp_cd();
		return 0;
		break;
	case 1:
		sp_help();
		return 0;
		break;
	case 2:
		sp_exit();
		return 0;
		break;
	default:
		return -1;
		break;
	}
	
}

//执行外部命令 需要用到管道来实现循环 子进程执行新的外部 父进程仍然执行当前的Shell
//In this project, command including pipe and redirection is called
//complicated command such as ls<a.txt>b.txt , ls|grep a and so on.
//And complicated command will be split into simple command like ls , a.txt , b.txt .
//Strictly speaking , files like a.txt are not commands which can be executed .
//So , files will not be executed as command but redirected to standard input or output
//of last simple comand . What’s more , builtin commands don’t support pipe and redirection.


int fd[2];//匿名管道
int sp_execute()
{
	int pid,localPtr;

	if('|'==pool->cmdPool[pool->cmdPtr].nextSign) 
	{	
		localPtr=pool->cmdPtr;
		pipe(fd);

		pid=fork();

		if(0==pid)//execute next command in child process
		{
			pool->cmdPtr++;

			dup2(fd[0],0);//重定向至标准输入
			close(fd[0]);
			close(fd[1]);

			sp_execute();
			exit(EXIT_SUCCESS);
		}
		else//execute current command in current process 父进程
		{
			signal(SIGCHLD, SIG_IGN);

			dup2(fd[1],1);//redirect standard output to pipe(write) 重定向至标准输出
			close(fd[0]);
			close(fd[1]);

			if(0!=execvp(pool->cmdPool[localPtr].cmd[0],pool->cmdPool[localPtr].cmd))
				printf("No such command!\n");//找不到命令

			exit(EXIT_SUCCESS);
		}
	}
	else if('<'==pool->cmdPool[pool->cmdPtr].nextSign)//重定向的情况
	{
		localPtr=pool->cmdPtr;
		char fileName[20];
		
		strcpy(fileName,pool->cmdPool[localPtr+1].cmd[0]);
		freopen(fileName,"r",stdin);//redirect stdin to fileName

		if('>'==pool->cmdPool[pool->cmdPtr+1].nextSign)
		{
			strcpy(fileName,pool->cmdPool[localPtr+2].cmd[0]);
			freopen(fileName,"w",stdout);//redirect stdout to fileName
		}
		if('|'==pool->cmdPool[pool->cmdPtr+1].nextSign)
		{
			pipe(fd);
			pid=fork();
			if(0==pid)//execute next command in child process
			{
				pool->cmdPtr+=2;

				dup2(fd[0],0);//redirect standard input to pipe(read)
				close(fd[0]);
				close(fd[1]);

				sp_execute();
				exit(EXIT_SUCCESS);
			}
			else//execute current command in current process
			{
				
				signal(SIGCHLD, SIG_IGN);

				dup2(fd[1],1);//redirect stdout to pipe(write)
				close(fd[0]);
				close(fd[1]);

				if(0!=execvp(pool->cmdPool[localPtr].cmd[0],pool->cmdPool[localPtr].cmd))
					printf("No such command!\n");

				exit(EXIT_SUCCESS);
			}
			
		}
		if(0!=execvp(pool->cmdPool[localPtr].cmd[0],pool->cmdPool[localPtr].cmd))
			printf("No such command!\n");

		exit(EXIT_SUCCESS);
		
	}
	else if('>'==pool->cmdPool[pool->cmdPtr].nextSign)
	{
		localPtr=pool->cmdPtr;
		char fileName[20];

		strcpy(fileName,pool->cmdPool[localPtr+1].cmd[0]);
		freopen(fileName,"w",stdout);//redirect stdout to fileName
		if(0!=execvp(pool->cmdPool[localPtr].cmd[0],pool->cmdPool[localPtr].cmd))
			printf("No such command!\n");
		
		exit(EXIT_SUCCESS);
	}
	else
	{
		localPtr=pool->cmdPtr;
		if(0!=execvp(pool->cmdPool[localPtr].cmd[0],pool->cmdPool[localPtr].cmd))
			printf("No such command!\n");

		exit(EXIT_SUCCESS);
	}

	return 0;

}


//清空内存池
int sp_clearCmdPool(cmdPoolStruct * pool)
{
	int i,j;

	pool->cmdPtr=0;
	for(i=0;i<POOL_SZ;i++)
	{
		for(j=0;j<CMD_LENG;j++)
		{
			pool->cmdPool[i].cmd[j]=NULL;
			pool->cmdPool[i].cmdStr[0]='\0';
		}
		pool->cmdPool[i].nextSign=0;
	}
	
	return 0;
}

//主函数
cmdPoolStruct * pool;

int main()
{
	char line[LINE_MAX];
	int pid;
	//初始化
	sp_shm_init();
	sp_init();
	while(1)
	{
		sp_prompt();//提示符样式

		sp_readLine(line);//输入命令

		sp_analyCmd(line);//解析命令

		if(0==sp_builtinCmd())//执行内部命令           
		{
			sp_clearCmdPool(pool);
			continue;
		}
		//fork a new process 执行外部命令
		else
		{
			pid = fork();
			if(0==pid)//子进程执行外部命令 原来的父进程就继续执行shell的其他的代码，比如等待命令执行完成、等待用户输入其他命令
			{
				sp_execute();
			}
			waitpid(pid,NULL,0);//等待结束
		}

		sleep(1); 		 //It is necessary to wait for long enough time before clear command pool
		sp_clearCmdPool(pool);//Because ,it doesn't use method of process synchronization to ensure
		//that clearing command is after all the command having been executed in other descendant process 
	}
	return 0;

} 