#include "parse.h"
#include <string.h>
#include <stdlib.h>

// Split a string into several substring divided by separator, and return the number of substring.
// e.g. 1,2 -> 1 2
char** Split(char* str, char delim, int* retCount)
{
	int count = 1;
	char* str_copy = str;
	while (*str_copy)
	{
		if (*str_copy == delim)
			count++;
		str_copy++;
	}
	str_copy = str;
	int* allLengths = malloc(count * sizeof(int));
	int currentLength = 0;
	int index = 0;
	while (*str_copy)
	{
		if (*str_copy == delim)
		{
			allLengths[index++] = currentLength;
			currentLength = 0;
		}
		else
			currentLength++;
		str_copy++;
	}
	allLengths[index++] = currentLength;
	int i = 0;
	str_copy = str;
	char** result = malloc(count * sizeof(char*));
	for (; i < count; i++)
	{
		char* substr = malloc((allLengths[i] + 1) * sizeof(char));
		result[i] = substr;
		int j = 0;
		for (; j < allLengths[i]; j++)
		{
			substr[j] = *str_copy;
			str_copy++;
		}
		substr[j] = '\0';
		str_copy++;
	}
	if(retCount)
		*retCount = count;
	free(allLengths);
	return result;
}

//Return the substring from a string, from start to end
char* SubStr(char *string, int start, int length)
{
	int len = strlen(string);
	if (length > len)
		length = len;
	if (start < 0)
	{
		start = len + start;
		if (start < 0)
			start = 0;
	}
	if (length <= 0)
	{
		length = len + length;
		if (length < start)
			length = len;
	}
	if (start + length > len || start < 0)
		start = 0;
	char * substring = (char*)calloc(strlen(string) + 1, sizeof(char));
	strncpy(substring, &string[start], length);
	return substring;
}

//Seperate redirection symbol from command string e.g. cat<file1>file2 -> cat <file1 >file2
char* SeperateRedirection(char* cmd)
{
	//Seperate <
	int i = 0;
	int len = strlen(cmd);
	for (; i < len; i++)
	{
		if (cmd[i] == '<' && i > 0 && cmd[i - 1] != ' ')
		{
			char* copy = malloc(len + 1);
			strcpy(copy, cmd);
			free(cmd);
			cmd = malloc(len + 1 + 1);
			strcpy(cmd, SubStr(copy, 0, i));
			strcat(cmd, " ");
			strcat(cmd, SubStr(copy, i, len - i));
			free(copy);
			break;
		}
	}

	//Seperate >
	i = 0;
	len = strlen(cmd);
	for (; i < len; i++)
	{
		if (cmd[i] == '>' && i > 0 && cmd[i - 1] != ' ')
		{
			char* copy = malloc(len + 1);
			strcpy(copy, cmd);
			free(cmd);
			cmd = malloc(len + 1 + 1);
			strcpy(cmd, SubStr(copy, 0, i));
			strcat(cmd, " ");
			strcat(cmd, SubStr(copy, i, len - i));
			free(copy);
			break;
		}
	}
	return cmd;
}

//Detect whether running in background
int DetectBackground(char** str)
{
	int length = strlen(*str);
	while ((*str)[length] == ' ' || (*str)[length] == '\n' || (*str)[length] == '\0') //Ignore the empty characters in the back
		length--;
	if ((*str)[length] == '&')
	{
		*str = SubStr(*str, 0, length);
		return 1;
	}
	return 0;
}

//Handle single quote
void SingleQuote(struct Command* cmd)
{
	int i = 0;
	for (; i < cmd->argCount; i++)
	{
		if (cmd->arguments[i][0] == '\'')
		{
			//Find whether there is another single quote in the same argument
			int len = strlen(cmd->arguments[i]);
			int foundSingleQuote = 0;
			if (cmd->arguments[i][len - 1] == '\'')
				foundSingleQuote = 1;
			if (foundSingleQuote)
			{
				if (len - 1 == 1) // If find pattern ''
					cmd->arguments[i][0] = '\0';
				else
					cmd->arguments[i] = SubStr(cmd->arguments[i], 1, len - 2); //Remove the first and last ' e.g. 'str'->str
			}
			else
			{
				//Find the corresponding back single quote
				int end = i + 1;
				int totalLength = strlen(cmd->arguments[i]);
				for (; end < cmd->argCount; end++)
				{
					len=strlen(cmd->arguments[end]);
					totalLength += len;
					if (cmd->arguments[end][len - 1] == '\'')
						break;
				}
				//Combine the arguments which belong to a pair of single quotes
				cmd->arguments[i] = realloc(cmd->arguments[i], totalLength + end - i + 1);
				int k = i + 1;
				for (; k <= end; k++)
				{
					strcat(cmd->arguments[i], " ");
					strcat(cmd->arguments[i], cmd->arguments[k]);
				}
				cmd->arguments[i] = SubStr(cmd->arguments[i], 1, strlen(cmd->arguments[i]) - 2);//Remove the first and last ' e.g. 'str'->str
				//Move forward the arguments behind the corresponding back single quote
				k = end + 1;
				for (; k < cmd->argCount; k++)
					cmd->arguments[i + k - end] = cmd->arguments[k];
				cmd->argCount -= end - i; //Refresh the argument count
				cmd->arguments[cmd->argCount] = NULL;
			}
		}
	}
}

//Analyse the command line
struct Command* Parse(char* str, int* count)
{
	//Ignore #
	if (str[0] == '#') //If first character is #
		return NULL;
	str = Split(str, '#', NULL)[0];
	//Seperate < and >
	str = SeperateRedirection(str);
	//Whether running in background
	int isBackground = DetectBackground(&str);
	//Split by pipeline to find each command
	char** cmdStrs = Split(str, '|', count);
	struct Command* allCmds = calloc(*count, sizeof(struct Command));
	int i = 0;
	for (; i < *count; i++)
	{
		int wordCount = 0;
		//Split by space to find command name, arguments, and redirections
		char** wordStrs = Split(cmdStrs[i], ' ', &wordCount);
		int j = 0;
		//No space before command name
		if (wordStrs[0][0] == '\0')
			j = 1;
		int argIndex = 0;

		int foundName = 0;//Whether find the command name
		for (; j < wordCount; j++)
		{
			//Found "<"
			if (wordStrs[j][0] == '<')
			{
				//If redirection symbol has a space in the back, e.g. < File rather than <File
				if (wordStrs[j][1] == '\0')
					allCmds[i].inputRedirect = wordStrs[++j];
				else
				{
					int length = strlen(wordStrs[j]);
					allCmds[i].inputRedirect = SubStr(wordStrs[j], 1, length - 1);
				}
			}
			//Found ">"
			else if (wordStrs[j][0] == '>')
			{
				//If redirection symbol has a space in the back, e.g. < File rather than <File
				if (wordStrs[j][1] == '\0')
					allCmds[i].outputRedirect = wordStrs[++j];
				else if (wordStrs[j][1] == '>') //If append, which has >>
				{
					allCmds[i].isAppend = 1;
					if (wordStrs[j][2] == '\0') //If redirection symbol has a space in the back, e.g. << File rather than <<File
						allCmds[i].outputRedirect = wordStrs[++j];
					else //If redirection symbol has no space in the back, e.g. <<File rather than << File
					{
						int length = strlen(wordStrs[j]);
						allCmds[i].outputRedirect = SubStr(wordStrs[j], 2, length - 2);
					}
				}
				else //Normal output redirection
				{
					allCmds[i].isAppend = 0;
					int length = strlen(wordStrs[j]);
					allCmds[i].outputRedirect = SubStr(wordStrs[j], 1, length - 1);
				}
			}
			//Common argument
			else
			{
				//If we still did not add the command name
				if (!foundName)
				{
					allCmds[i].name = malloc(strlen(wordStrs[j]) + 1);
					strcpy(allCmds[i].name, wordStrs[j]);
					//Add argv[0] into argument
					allCmds[i].arguments[argIndex] = malloc(strlen(wordStrs[j]) + 1);
					strcpy(allCmds[i].arguments[argIndex], wordStrs[j]);
					argIndex++;
					foundName = 1;
				}
				//If the argument is not empty and found command name
				else if (wordStrs[j][0] != '\0')
				{
					allCmds[i].arguments[argIndex] = malloc(strlen(wordStrs[j]) + 1);
					strcpy(allCmds[i].arguments[argIndex], wordStrs[j]);
					argIndex++;
				}
			}
		}
		allCmds[i].arguments[argIndex] = NULL;
		allCmds[i].argCount = argIndex;
		allCmds[i].isBackground = isBackground;
		SingleQuote(&allCmds[i]);
	}
	return allCmds;
}