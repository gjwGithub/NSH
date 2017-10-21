#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include "parse.h"

int lastStatus = 0; //Last exit status
int backgroundIndex = 1; //Index of background command
int lastPid = 0; //PID of the last executed command

//Output the parsing result
void OutputParse(struct Command* cmds, int count)
{
	printf("%d: ", count);
	int i = 0;
	for (; i < count; i++)
	{
		if (cmds[i].inputRedirect)
			printf("< '%s' ", cmds[i].inputRedirect);
		printf("'%s' ", cmds[i].name);
		int j = 1;
		for (; j < cmds[i].argCount; j++)
			printf("'%s' ", cmds[i].arguments[j]);
		if (cmds[i].outputRedirect)
			printf("> '%s' ", cmds[i].outputRedirect);
		if (i != count - 1)
			printf("| ");
	}
	printf("\n");
}

//Execute the command
void Execute(struct Command* cmd, int* pipefds, int index, int count)
{
	int pid = fork();
	lastPid = pid; //Refresh the latest pid number
	if (pid < 0)
	{
		perror("Fork error");
		exit(1);
	}
	else if (pid == 0)//Child process
	{
		//Redirect stdin, start from 2nd command
		if (index > 0)
		{
			if (dup2(pipefds[(index - 1) * 2], 0) < 0)
			{
				perror("DUP2 error");
				exit(-1);
			}
		}
		//If custom redirection for stdin exists
		if (cmd->inputRedirect)
		{
			if (!strcmp(cmd->inputRedirect, "0"))
			{
				if (dup2(0, 0) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else if (!strcmp(cmd->inputRedirect, "1"))
			{
				if (dup2(1, 0) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else if (!strcmp(cmd->inputRedirect, "2"))
			{
				if (dup2(2, 0) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else
			{
				int fd = open(cmd->inputRedirect, O_RDONLY);
				if (dup2(fd, 0) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
				close(fd);
			}
		}
		//Redirect stdout
		if (index < count - 1)
		{
			if (dup2(pipefds[index * 2 + 1], 1) < 0)
			{
				perror("DUP2 error");
				exit(-1);
			}
		}
		//If custom redirection for stdout exists
		if (cmd->outputRedirect)
		{
			if (!strcmp(cmd->outputRedirect, "0"))
			{
				if (dup2(0, 1) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else if (!strcmp(cmd->outputRedirect, "1"))
			{
				if (dup2(1, 1) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else if (!strcmp(cmd->outputRedirect, "2"))
			{
				if (dup2(2, 1) < 0)
				{
					perror("DUP2 error");
					exit(-1);
				}
			}
			else
			{
				if (cmd->isAppend == 0) //If not append to the file
				{
					int fd = creat(cmd->outputRedirect, S_IWUSR);
					if (dup2(fd, 1) < 0)
					{
						perror("DUP2 error");
						exit(-1);
					}
					close(fd);
				}
				else
				{
					int fd = open(cmd->outputRedirect, O_WRONLY | O_APPEND);
					if (dup2(fd, 1) < 0)
					{
						perror("DUP2 error");
						exit(-1);
					}
					close(fd);
				}
			}
		}
		int i = 0;
		for (i = 0; i < 2 * (count - 1); i++)
			close(pipefds[i]);

		lastStatus = execvp(cmd->name, cmd->arguments);
		perror("Exec error");
		exit(lastStatus);
	}
}

//Handle command "cd"
void HandleCd(struct Command* cmd)
{
	if (cmd->argCount == 1) //If "cd", go to ~
	{
		struct passwd *pw = getpwuid(getuid());
		const char *homedir = pw->pw_dir;
		lastStatus = chdir(homedir);
	}
	else
		lastStatus = chdir(cmd->arguments[1]);
}

//Handle command "exit"
void HandleExit(struct Command* cmd)
{
	if (cmd->argCount == 1)
	{
		fprintf(stderr, "Exit with code: %d\n", lastStatus);
		exit(lastStatus);
	}
	else
	{
		int num = atoi(cmd->arguments[1]);
		fprintf(stderr, "Exit with code: %d\n", num);
		exit(num);
	}
}

void HandleLine(char* line)
{
	char* input = SubStr(line, 0, strlen(line) - 1); //Remove \n
	int count = 0;
	struct Command* cmds = Parse(input, &count);

	if (!cmds) //If no command
		return;
	if (!strcmp(cmds[0].name, "cd"))
		HandleCd(&cmds[0]);
	else if(!strcmp(cmds[0].name, "exit"))
		HandleExit(&cmds[0]);
	else
	{
		// create pipe for communication
		int numPipes = count - 1;
		int pipefds[2 * numPipes];
		int i = 0;
		for (i = 0; i < numPipes; i++)
		{
			if (pipe(pipefds + i * 2) < 0)
			{
				perror("Pipe error");
				exit(1);
			}
		}

		i = 0;
		for (; i < count; i++)
			Execute(&cmds[i], pipefds, i, count);
		for (i = 0; i < 2 * numPipes; i++)
			close(pipefds[i]);
		if (!cmds[0].isBackground)
			for (i = 0; i < numPipes + 1; i++)
				wait(&lastStatus);
		else
			printf("[%d] %d\n", backgroundIndex++, lastPid);

		//OutputParse(cmds, count);
		free(cmds);
	}
}

//Signal Handler for SIGINT
void sigintHandler(int sig_num)
{
	return;
}

int main(int argc, char* argv[])
{
	signal(SIGINT, sigintHandler); //Ignore control-c

	if (argc == 1) //Go into prompt interface
	{
		char* line = NULL;
		size_t len = 0;
		ssize_t read;

		while (1)
		{
			printf("? ");
			read = getline(&line, &len, stdin);
			if (read == -1) //Exit when input control-d
				break;
			if (line[0] == '\n') //Ignore enter
				continue;
			HandleLine(line);
		}
	}
	else //Execute script file
	{
		FILE* script = fopen(argv[1], "r");
		char* line = NULL;
		size_t len = 0;
		ssize_t read;

		while (1)
		{
			read = getline(&line, &len, script);
			if (read == -1) //Exit when reads EOF
				break;
			if (line[0] == '\n') //Ignore enter
				continue;
			HandleLine(line);
		}

		fclose(script);
	}

	return 0;
}