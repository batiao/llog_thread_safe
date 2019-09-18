#ifndef __LLINI_H__
#define __LLINI_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_BUFFER_LEN 1024
#define SECTION_BUFFER_LEN 256

typedef int (*LINE_PARSE_FUN)(void *userData, const char *section, const char *name, const char *value);

int ini_parse_file(FILE *fileFD, LINE_PARSE_FUN getLineFun, void *userData)
{
	char *lineBuff;//[DEFAULT_BUFFER_LEN];
	char *curLinePtr;
	char *startData;
	char *endData;
	size_t buffLen;
	char sectionBuff[SECTION_BUFFER_LEN];
	uint16_t maxLineLen;

	lineBuff = (char *)malloc(sizeof(char) * DEFAULT_BUFFER_LEN);
	if(!lineBuff) {
		//printf("malloc fail\n");
		return -1;
	}

	maxLineLen = DEFAULT_BUFFER_LEN;
	sectionBuff[0] = '\0';
	while(fgets(lineBuff, maxLineLen, fileFD) != NULL) {
		buffLen = strlen(lineBuff);
		while((buffLen == maxLineLen - 1) && (lineBuff[maxLineLen - 2] != '\n')) {
			//长度不够
			maxLineLen += (DEFAULT_BUFFER_LEN/2);
			lineBuff = realloc(lineBuff, maxLineLen);
			if(!lineBuff) {
				//printf("realloc fail\n");
				return 1;
			}
			curLinePtr = lineBuff + buffLen;
			if(fgets(curLinePtr, DEFAULT_BUFFER_LEN/2 + 1, fileFD) == NULL) break;
			buffLen += strlen(curLinePtr);
		}
		//printf("kkkkk=%s\n", lineBuff);
		if(lineBuff[0] == '#' || lineBuff[0] == '\n') {
			continue;//注释行 空行
		}
		else if(lineBuff[0] == '[') {
			//找段标识
			startData = lineBuff + 1;
			if(!(endData = strchr(startData, ']'))) {
				//printf("get bad section data\n");
				continue;
			}
			*endData = '\0';
			if(SECTION_BUFFER_LEN <= endData - startData) {
				//printf("section too long\n");
				continue;
			}
			strcpy(sectionBuff, startData);
		}
		else {
			//解析内容
			if(!(endData = strchr(startData, '='))) {
				//printf("get bad line\n");
				continue;
			}
            buffLen--;
            if(lineBuff[buffLen] == '\n') {
                if(lineBuff[buffLen - 1] == '\r') {
                    buffLen--;
                }
                lineBuff[buffLen] = '\0';
            }
			startData = lineBuff;
			*endData = '\0';
			endData++;
			getLineFun(userData, sectionBuff, startData, endData);
		}
	}
	
	free(lineBuff);
	return 0;
}

int parse_ini(const char *fileName, LINE_PARSE_FUN funName, void *userData)
{
	FILE *fileFD = fopen(fileName, "r");
    if(!fileFD) {
        return -1;
	}
    if(ini_parse_file(fileFD, funName, userData)) {
		fclose(fileFD);
		return 1;
	}
    fclose(fileFD);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
