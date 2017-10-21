#pragma once

struct Command
{
	char* name;
	int argCount;
	char* arguments[128];
	char* inputRedirect;
	char* outputRedirect;
	int isAppend;
	int isBackground;
};

char* SubStr(char *string, int start, int length);

struct Command* Parse(char* str, int* count);
