#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "parse.h"

#define BUFFERSIZE 8192

char* lastResult = NULL; //Store the output result in the latest command
int lastResultSize = 0; //Size of output result in the latest command

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
		for(;j<cmds[i].argCount;j++)
			printf("'%s' ", cmds[i].arguments[j]);
		if (cmds[i].outputRedirect)
			printf("> '%s' ", cmds[i].outputRedirect);
		if (i != count - 1)
			printf("| ");
	}
	printf("\n");
}

//Read last output result into input stream
void LoadInput(int fd)
{
	char* iter = lastResult;
	//If last result is empty
	if (!iter)
		return;
	//FILE* stream = fdopen(fd, "w");
	//while (*iter != '\0') 
	//{
	//	fputc(*iter++, stream);
	//}
	int n = write(fd, lastResult, lastResultSize);
	fprintf(stderr, "--------\n");
	fprintf(stderr, "83: %d\n", lastResult[83]);
	fprintf(stderr, "last: %d %d\n %s\n", n, lastResultSize, lastResult);
	fprintf(stderr, "--------\n");
}

int Execute(struct Command* cmd, int lastResultLength)
{
	int outPipe[2];
	int inPipe[2];
	// create pipe for communication
	if (pipe(outPipe) != 0)
	{
		perror("Out Pipe error");
		exit(1);
	}
	if (pipe(inPipe) != 0)
	{
		perror("In Pipe error");
		exit(1);
	}
	int pid = fork();
	int totalBytes = 0;
	if (pid < 0)
	{
		perror("Fork error");
		exit(1);
	}
	else if (pid == 0)//Child process
	{
		//close(STDOUT_FILENO);
		fprintf(stderr, "1\n");
		dup2(outPipe[1], STDOUT_FILENO);
		close(outPipe[0]);
		//close(outPipe[1]);

		fprintf(stderr, "2\n");
		if (lastResultLength == 0) //First command
			execvp(cmd->name, cmd->arguments);
		else
		{
			LoadInput(inPipe[1]);
			close(inPipe[1]);
			dup2(inPipe[0], STDIN_FILENO);
			
			//close(inPipe[0]);

			/*int fd = open("test", O_RDONLY);
			dup2(fd, 0);*/

			/*char buff[255];
			read(inPipe[0], buff, 255);
			fprintf(stderr, "test: %s\n", buff);*/

			close(inPipe[0]);
			close(outPipe[0]);
			close(outPipe[1]);

			execvp(cmd->name, cmd->arguments);
		}
		perror("Exec error");
		exit(-1);
	}
	else//Parent process
	{
		

		fprintf(stderr, "3\n");
		close(outPipe[1]);

		int i = 1;
		int nbytes = 0;
		do
		{
			char* buffer = malloc(BUFFERSIZE);
			nbytes = read(outPipe[0], buffer, BUFFERSIZE);	
			if (i == 1)
			{
				//Add the result
				lastResult = malloc(BUFFERSIZE);
				strcpy(lastResult, buffer);
			}
			else
			{
				//Append the result
				lastResult = realloc(lastResult, BUFFERSIZE * i);
				strcat(lastResult, buffer);
			}	
			totalBytes += nbytes;
			i++;
			free(buffer);
		} while (nbytes == BUFFERSIZE);

		lastResult = realloc(lastResult, BUFFERSIZE * i + 1);
		//lastResult[totalBytes] = -1;
		lastResult[++totalBytes] = -1;

		close(outPipe[0]);
		fprintf(stderr, "4\n");
		
	}
	return totalBytes;
}

int main()
{
	char* line = NULL;
	size_t len = 0;
	ssize_t read;

	while(1)
	{
		printf("? ");
		read = getline(&line, &len, stdin);
		if (read == -1)
			break;
		//Remove \n
		char* input = SubStr(line, 0, strlen(line) - 1);
		int count = 0;
		struct Command* cmds = Parse(input, &count);

		//int pid = fork();
		//if (pid < 0)
		//{
		//	perror("Fork error");
		//	exit(1);
		//}
		//else if (pid == 0)//Child process
		//{
		//	int i = 0;
		//	for (; i < count; i++)
		//	{
		//		Execute(&cmds[i]);
		//	}
		//}
		//else//Parent process
		//{
		//	wait(NULL);
		//}

		int i = 0;
		for (; i < count; i++)
		{
			//int w = write(STDIN_FILENO, "sdf\n", 5);
			//printf("%d\n", w);
			//execlp("wc", "", NULL);
			
			lastResultSize = Execute(&cmds[i], lastResultSize);
			if (lastResultSize == -1)
				return 0;

		}
		wait(NULL);
		wait(NULL);
		printf("result: %s\n", lastResult);
		//OutputParse(cmds, count);
		lastResultSize = 0;
		free(lastResult);
		free(cmds);
	}
	
    return 0;
}