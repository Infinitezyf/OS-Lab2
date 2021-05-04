#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PRINT_BLUE(s)  printf("\033[0;34m%s\033[0;39m",s);

//当前目录 全局变量  current directory of Shell
char dirPromt[20];

//根据指定字符串分割
int SplitStr(char *resultArr[], char *str , char *split)
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

	//getcwd()会将当前工作目录的绝对路径复制到参数buffer所指的内存空间中,参数maxlen为buffer的空间大小
	getcwd(dir,200);

	SplitStr(dirArr,dir,"/");

	//find current directory string
	for(i=0; ;i++)
	{
		if(NULL == dirArr[i])
			break;
	}

	strcpy( dirPromt , dirArr[i-1] );

	return 0;
}


//内部命令1——help
int Zyf_Help(void)
{
	printf("This is ZYF's Shell~\n");
	printf("What can I do for you?\n");
	printf("Type 'exit' to exit .\n");
	return 0;
}

//内部命令2——退出
int Zyf_exit(void)
{
	printf("exiting...\n");
	return 0;
}

//提示符样式
int Zyf_Prompt()
{
	PRINT_BLUE(dirPromt);
	PRINT_BLUE("$");
	return 0;
}

int main()
{
    int arg_num = 0;
    char command[100];
    while(1)
    {
    Zyf_Prompt();
    system("pwd");
        gets(command);
        if( !strcmp(command, "help"))
        {
           Zyf_Help();
            continue;
        }

        else if( !strcmp(command, "exit") )
        {
            Zyf_exit();
            break;
        }
	else if( command[0] =='c' && command[1] =='d' && command[2] ==' '  )
	{
	    char *token = " ";
	    char *dir;
	    dir = strtok(command,token);
	    dir = strtok(NULL, token);
	    chdir(dir);
	    continue;
	}
	
	int pid = fork();
	if(0==pid)//子进程执行外部命令 原来的父进程就继续执行shell的其他的代码，比如等待命令执行完成、等待用户输入其他命令
	{
		char *p;
		p = (char *)malloc(100*sizeof(char));
		strcpy(p, command);
        	system(p);
	}
	waitpid(pid,NULL,0);//等待结束
	
    }
}

