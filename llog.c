#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "llog.h"
#include "llini.h"

#define ONE_MEGA_BYTE               (1024 * 1024) //1MB
#define WRITE_LOG_CACHE_BUFF_LEN    (5 * 1024) //5KB

#define LOG_SECTION_NAME	"log_config"
#define LOG_PRINTF_LEVEL 	"logLevel"
#define LOG_FILE_PATH		"logPath"
#define LOG_FILE_MAX_SIZE	"logFileMaxSize"
#define LOG_MAX_COUNT		"logMaxCount"
#define LOG_FLUSH_SIZE		"logFlushSize"
#define LOG_FLUSH_ITVAL		"logFlushInterval"
#define LOG_UPDATA_ITVAL	"logUpdateInterval"

#define DEFALUT_LOG_LEVEL 		    LOG_LEVEL_WARN
#define DEFALUT_LOG_PATH 		    "/home/my/test.log"
#define DEFALUT_LOG_MAX_COUNT	    30
#define DEFALUT_LOG_FILE_MAX_SIZE	30//MB
#define DEFALUT_LOG_FLUSH_SIZE      1//MB
#define DEFALUT_LOG_FLUSH_ITVAL	    2
#define DEFALUT_LOG_UPDATA_ITVAL    0

#define MAX_FILE_PATH_LEN			1024

#define LLOG_WORK_THREAD_STOP		0
#define LLOG_WORK_THREAD_START		1

#define MAX_CACHE_COUNTS            5

#define LLOG_ERR_PRINTF(format, ...)    fprintf(stderr, format, ##__VA_ARGS__);

typedef enum LOG_CACHE_STATE_E
{
    EMPTY_STATE = 1,
	USING_STATE,
	UPDATE_STATE
} CACHE_STATE_ENUM;

typedef struct LLOG_CONFIG_STRUCT_T
{
	LOG_ENUM logLevel; //log�ȼ�
	char *logPath; //log���ɵ�·��
	uint32_t fileCount; //������־�ĸ���
	uint32_t flushInterval; //������ˢ�¼��(΢��)
	uint32_t updateInterval; //�������õļ��
	uint32_t maxFileSize; //�ļ����λ
	uint32_t flushSize; //��������С
	
} LLOG_CONFIG_STRUCT;

typedef struct LLOG_LOGFILE_INFO_T
{
    FILE* logFD; //��ǰ��־�ļ����
    uint32_t fileCount; //��ǰ��־�ļ��ļ���
    
    uint32_t fileSize; //��ǰ��־�ļ���С

    CACHE_STATE_ENUM *cacheStat;
    uint32_t *cacheLen; //��ǰ��������ʹ�õĳ���
    char *logCachePtr; //��ǰʹ�õĻ�����

	char *newFilePath; //��־�ļ�������
	char *oldFilePath; //��־�ļ�������
	char *newFileFlagSite; //�ļ���������־
	char *oldFileFlagSite; //�ļ���������־
    
    pthread_t threadId; //��־�����߳�ID
    pthread_cond_t cond; //�ȴ���������
    pthread_mutex_t condMutex; //��֪ͨ�����á���cond���ʹ��
    pthread_mutex_t buffMutex; //��LOG��������
	
} LLOG_LOGFILE_INFO;

typedef struct LLOG_CACHE_BUFF_T
{
    char *buffPtr;
    uint32_t buffLen;
    CACHE_STATE_ENUM state;
} LLOG_CACHE_BUFF;

typedef struct LLOG_ALL_INFO_T
{
    uint8_t             exitLogThread;  //�����߳���Դ
    LLOG_CONFIG_STRUCT  configInfo;
    LLOG_LOGFILE_INFO   currentLogFile;
    LLOG_CACHE_BUFF     logCache[MAX_CACHE_COUNTS];
} LLOG_ALL_INFO;

static LLOG_ALL_INFO g_llog;

#define FROME_CURRENT_TIME_STRING(A, B) ({\
    time_t timer = time(NULL);\
    strftime((A), (B), "%Y-%m-%d %H:%M:%S", localtime(&timer));\
    })

#define GET_FIGURE_CONFIG_FUN(A, B, C) ({\
    int tempInt = atoi((B));\
	if(tempInt <= 0 || tempInt >= UINT8_MAX) {\
		tempInt = (C);\
	}\
	(A) = abs(tempInt);\
    })

#define PRE_PRO_TAG "%04d-%02d-%02d %02d:%02d:%02d [%s:%s:%d] %s "
#define VAL_PRO_TAG ,pTM->tm_year+1900,pTM->tm_mon+1,pTM->tm_mday,pTM->tm_hour,pTM->tm_min,pTM->tm_sec,__FILE__,__FUNCTION__,__LINE__

#define PRE_TAG "%04d-%02d-%02d %02d:%02d:%02d %s "
#define VAL_TAG ,pTM->tm_year+1900,pTM->tm_mon+1,pTM->tm_mday,pTM->tm_hour,pTM->tm_min,pTM->tm_sec

//#include "llog.h"

#define CHECK_AND_TIME_FROM(A) if((A) >= g_llog.configInfo.logLevel){\
    struct tm * pTM;\
    time_t timer = time(NULL);\
	pTM = localtime(&timer);
    
#define LLOG_FORMAT_PRO(levelNum, logLevel, format, ...) ({\
    CHECK_AND_TIME_FROM(levelNum)\
    llog_form_string_to_buff(PRE_PRO_TAG format VAL_PRO_TAG,logLevel, ##__VA_ARGS__);\
    }\
    })

#define LLOG_FORMAT(levelNum, logLevel, format, ...) ({\
    CHECK_AND_TIME_FROM(levelNum)\
    llog_form_string_to_buff(PRE_TAG format VAL_TAG,logLevel, ##__VA_ARGS__);\
    }\
    })

#define INIT_LOGINFO_SET_LOCK(fileHand) ({\
	pthread_mutex_init(&((fileHand).buffMutex), NULL);\
	pthread_mutex_init(&((fileHand).condMutex), NULL);\
	pthread_cond_init(&((fileHand).cond), NULL);\
	})

#define CREATE_NEW_LOG_FILE(llogHandle) ({\
    int i;\
	for(i = (llogHandle)->currentLogFile.fileCount; i >= 0; i--) {\
		sprintf((llogHandle)->currentLogFile.oldFileFlagSite, ".%d", i);\
		sprintf((llogHandle)->currentLogFile.newFileFlagSite, ".%d", i + 1);\
		rename((llogHandle)->currentLogFile.oldFilePath, (llogHandle)->currentLogFile.newFilePath);\
	}\
	sprintf((llogHandle)->currentLogFile.oldFileFlagSite, ".%d", 0);\
	rename((llogHandle)->configInfo.logPath, (llogHandle)->currentLogFile.oldFilePath);\
	(llogHandle)->currentLogFile.logFD = fopen((llogHandle)->configInfo.logPath, "w");\
	if(!(llogHandle)->currentLogFile.logFD) {\
		fprintf(stderr, "open %s fail\n", (llogHandle)->configInfo.logPath);\
		exit(1);\
	}\
	(llogHandle)->currentLogFile.fileSize = 0;\
	})

#define FIRST_READ_LOG_FILE(llogHandle) ({\
    (llogHandle)->currentLogFile.logFD = fopen((llogHandle)->configInfo.logPath, "w");\
    if(!(llogHandle)->currentLogFile.logFD) {\
		fprintf(stderr, "open %s fail\n", (llogHandle)->configInfo.logPath);\
		exit(1);\
	}\
	fseek((llogHandle)->currentLogFile.logFD, 0, SEEK_END);\
    (llogHandle)->currentLogFile.fileSize = ftell((llogHandle)->currentLogFile.logFD);\
    })

#define CREATE_LLOG_WORK_THREAD(A, B) ({\
	if(pthread_create(&(A), NULL, llog_thread_work_fun, (void*)&(B))) {\
		LLOG_ERR_PRINTF("create flush thread failed\n");\
		return 1;\
	}\
	})

#define DELETE_LOG_CATCH_BUFF(A, B) ({\
    int freeloop = 0;\
    for(; freeloop < (A); freeloop) {\
        free((B)->logCache[freeloop].buffPtr);\
    }\
    LLOG_ERR_PRINTF("init malloc fail\n");\
    })

#define GET_EMPTY_CACHE_FOR_WRITE_LOG() ({\
    int findCount;\
    for(;;) {\
        for(findCount = 0; findCount < MAX_CACHE_COUNTS; findCount++) {\
            if(g_llog.logCache[findCount].state != EMPTY_STATE) continue;\
            g_llog.logCache[findCount].state = USING_STATE;\
            g_llog.currentLogFile.logCachePtr = g_llog.logCache[findCount].buffPtr;\
            g_llog.logCache[findCount].buffLen = 0;\
            g_llog.currentLogFile.cacheLen = &g_llog.logCache[findCount].buffLen;\
            g_llog.currentLogFile.cacheStat = &g_llog.logCache[findCount].state;\
            break;\
        }\
        if(findCount < MAX_CACHE_COUNTS) break;\
        usleep(5000);\
    }\
    })

#define MATCH_CONDITION(A, B) (strcasecmp((A), (B)) == 0) 

int llog_get_config_fun(void *userData, const char *section, const char *name, const char *value)
{
	LLOG_CONFIG_STRUCT *dataBuff = (LLOG_CONFIG_STRUCT *)userData;
	if(MATCH_CONDITION(section, LOG_SECTION_NAME) && 
		MATCH_CONDITION(name, LOG_PRINTF_LEVEL)) {
		//��ȡ��־�ȼ�
		if(MATCH_CONDITION(value, "DEBUG")) {
			dataBuff->logLevel = LOG_LEVEL_DEBUG;
		}
		else if(MATCH_CONDITION(value, "INFO")) {
			dataBuff->logLevel = LOG_LEVEL_INFO;
		}
		else if(MATCH_CONDITION(value, "WARN")) {
			dataBuff->logLevel = LOG_LEVEL_WARN;
		}
		else if(MATCH_CONDITION(value, "ERROR")) {
			dataBuff->logLevel = LOG_LEVEL_ERROR;
		}
		else if(MATCH_CONDITION(value, "FATAL")) {
			dataBuff->logLevel = LOG_LEVEL_FATAL;
		}
		else {
			dataBuff->logLevel = DEFALUT_LOG_LEVEL;
		}
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_FILE_PATH)) {
		//��ȡ��־����Ŀ¼������
		char *tempPtr = (char *)malloc(strlen(value) + 1);
		strcpy(tempPtr, value);
		dataBuff->logPath = tempPtr;
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_FILE_MAX_SIZE)) {
		//��ȡ�����ļ����������
		GET_FIGURE_CONFIG_FUN(dataBuff->maxFileSize, value, DEFALUT_LOG_FILE_MAX_SIZE);
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_MAX_COUNT)) {
		//��ȡһ���ļ����Ĵ�С
		GET_FIGURE_CONFIG_FUN(dataBuff->fileCount, value, DEFALUT_LOG_MAX_COUNT);
        dataBuff->fileCount--;
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_FLUSH_SIZE)) {
		//��������С
		GET_FIGURE_CONFIG_FUN(dataBuff->flushSize, value, DEFALUT_LOG_FLUSH_SIZE);
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_FLUSH_ITVAL)) {
		//��������־��������
		GET_FIGURE_CONFIG_FUN(dataBuff->flushInterval, value, DEFALUT_LOG_FLUSH_ITVAL);
	}
	else if(MATCH_CONDITION(section, LOG_SECTION_NAME) &&
		MATCH_CONDITION(name, LOG_UPDATA_ITVAL)) {
		//��������ļ��Ƿ�仯����   =0  �����
		GET_FIGURE_CONFIG_FUN(dataBuff->updateInterval, value, DEFALUT_LOG_UPDATA_ITVAL);
	}

	return 0;
}

static inline 
int llog_write_buffer_to_file(const char *logBuff, uint32_t logLen, LLOG_ALL_INFO *logHandleInfo)
{
	//ѡ��fwrite����Ϊ����writeЧ�ʸ���
	fwrite(logBuff, logLen, 1, logHandleInfo->currentLogFile.logFD);
	fflush(logHandleInfo->currentLogFile.logFD);
	logHandleInfo->currentLogFile.fileSize += logLen;
	if(logHandleInfo->currentLogFile.fileSize < logHandleInfo->configInfo.maxFileSize) {
		return 1;
	}
	fclose(logHandleInfo->currentLogFile.logFD);
    if(logHandleInfo->currentLogFile.fileCount < logHandleInfo->configInfo.fileCount) {
        logHandleInfo->currentLogFile.fileCount++;
    }
	logHandleInfo->currentLogFile.logFD = NULL;
	
	return 0; //����log�ļ�
}

void llog_form_string_to_buff(const char *parameterA, ...)
{
	//static uint32_t recoredBuffLen = 0;
    
	int recordLogLen;
    uint32_t remainBuffLen;
	va_list argv;

	va_start(argv, parameterA);
    pthread_mutex_lock(&g_llog.currentLogFile.buffMutex);
    remainBuffLen = g_llog.configInfo.flushSize - *g_llog.currentLogFile.cacheLen;
	recordLogLen = vsnprintf(g_llog.currentLogFile.logCachePtr, remainBuffLen, parameterA, argv);
    g_llog.currentLogFile.logCachePtr += recordLogLen;
	*g_llog.currentLogFile.cacheLen += recordLogLen;
    if(remainBuffLen - recordLogLen <= WRITE_LOG_CACHE_BUFF_LEN) {
        *g_llog.currentLogFile.cacheStat = UPDATE_STATE;
        GET_EMPTY_CACHE_FOR_WRITE_LOG();
    }
    pthread_mutex_unlock(&g_llog.currentLogFile.buffMutex);
	va_end(argv);
    
    return;
}

void *llog_thread_work_fun(void *threadData)
{
    int sleepTime;
    int staticTime;
    int i;
    int writeFileResult;

    struct timeval startTime;
    struct timeval endTime;
    
    LLOG_ALL_INFO *logHandInfo = (LLOG_ALL_INFO *)threadData;

    staticTime = logHandInfo->configInfo.flushInterval * 1000000;
    sleepTime = staticTime;
	while(logHandInfo->exitLogThread != LLOG_WORK_THREAD_STOP) {
		gettimeofday(&startTime, NULL);

		usleep(sleepTime);
        for(i = 0; i < MAX_CACHE_COUNTS; i++) {
            if(logHandInfo->logCache[i].state == EMPTY_STATE) continue;
            if(logHandInfo->logCache[i].state == USING_STATE) {
                pthread_mutex_lock(&logHandInfo->currentLogFile.buffMutex);
                *logHandInfo->currentLogFile.cacheStat = UPDATE_STATE;
                GET_EMPTY_CACHE_FOR_WRITE_LOG();
                pthread_mutex_unlock(&logHandInfo->currentLogFile.buffMutex);
            }
            if(0 == logHandInfo->logCache[i].buffLen) {
                logHandInfo->logCache[i].state = EMPTY_STATE;
                continue;
            }
            writeFileResult = llog_write_buffer_to_file(logHandInfo->logCache[i].buffPtr, logHandInfo->logCache[i].buffLen, logHandInfo);
            logHandInfo->logCache[i].state = EMPTY_STATE;
            logHandInfo->logCache[i].buffLen = 0;
            if(writeFileResult) continue;
			CREATE_NEW_LOG_FILE(logHandInfo);
        }
        gettimeofday(&endTime, NULL);
        sleepTime = staticTime - ((endTime.tv_sec - startTime.tv_sec)*1000000 + (endTime.tv_usec - startTime.tv_usec));
        if(sleepTime < 0) {
            sleepTime = 0;
        }
	}

	return NULL;
}

static inline 
char *llog_check_log_file_path(char *filePath)
{
	do {
		char *tailPtr;
		char chrBuff;
		char processPath[MAX_FILE_PATH_LEN]; //[PATH_MAX];
		tailPtr = strrchr(filePath, '/');
		if(!tailPtr) {
			LLOG_ERR_PRINTF("bad log file path\n");
			break;
		}
		tailPtr++;
		chrBuff = *tailPtr;
		*tailPtr = '\0';

		if(access(filePath, F_OK) != 0) {
			LLOG_ERR_PRINTF("open log file folder fail\n");
			break;
		}
		*tailPtr = chrBuff;
		if('\0' != chrBuff) return filePath;

		if(readlink("/proc/self/exe", processPath, MAX_FILE_PATH_LEN) <= 0) {
			LLOG_ERR_PRINTF("get process path fail\n");
			break;
		}
		tailPtr = strrchr(processPath, '/');
		if(!tailPtr) {
			LLOG_ERR_PRINTF("bad log file path\n");
			break;
		}
		tailPtr++;
		filePath = (char *)realloc(filePath, 
			(sizeof(char) * (strlen(filePath) + strlen(tailPtr) + 2)));
		if(!filePath) {
			LLOG_ERR_PRINTF("realloc fail\n");
			break;
		}
		strcat(filePath, tailPtr);
		
		return filePath;
	} while(0);

	return NULL;
}

static inline
int llog_malloc_buff_for_remove_file(LLOG_ALL_INFO *llogHander)
{

	do {
		int mallocLen;
		int buffLen = strlen(llogHander->configInfo.logPath);
		mallocLen = buffLen + 8;
		llogHander->currentLogFile.newFilePath = (char *)malloc(sizeof(char) * mallocLen);
		if(!llogHander->currentLogFile.newFilePath) {
			LLOG_ERR_PRINTF("set_remove_file_buff malloc fail\n");
			break;
		}
		llogHander->currentLogFile.oldFilePath = (char *)malloc(sizeof(char) * mallocLen);
		if(!llogHander->currentLogFile.oldFilePath) {
			LLOG_ERR_PRINTF("set_remove_file_buff malloc fail\n");
			free(llogHander->currentLogFile.newFilePath);
			break;
		}
		strcpy(llogHander->currentLogFile.newFilePath, llogHander->configInfo.logPath);
		strcpy(llogHander->currentLogFile.oldFilePath, llogHander->configInfo.logPath);
		llogHander->currentLogFile.newFileFlagSite = llogHander->currentLogFile.newFilePath + buffLen;
		llogHander->currentLogFile.oldFileFlagSite = llogHander->currentLogFile.oldFilePath + buffLen;
		
		return 0;
	} while(0);
	llogHander->currentLogFile.newFilePath = NULL;
	llogHander->currentLogFile.oldFilePath = NULL;
	
	return 1;

}

int llog_init_work_parameter(LLOG_ALL_INFO *llogHander)
{

	do {
		char *tempPtr;
        int i;

		llogHander->configInfo.logPath = llog_check_log_file_path(llogHander->configInfo.logPath);
		if(!llogHander->configInfo.logPath) break;

		if(llog_malloc_buff_for_remove_file(llogHander)) break;

        llogHander->exitLogThread = LLOG_WORK_THREAD_START;
        llogHander->configInfo.maxFileSize = llogHander->configInfo.maxFileSize * ONE_MEGA_BYTE;
	    llogHander->configInfo.flushSize = llogHander->configInfo.flushSize * ONE_MEGA_BYTE + WRITE_LOG_CACHE_BUFF_LEN;

        for(i = 0; i < MAX_CACHE_COUNTS; i++) {
            llogHander->logCache[i].buffPtr = (char *)malloc(llogHander->configInfo.flushSize);
            if(NULL == llogHander->logCache[i].buffPtr) {
                DELETE_LOG_CATCH_BUFF(i + 1, llogHander);
                exit(1);
            }
            llogHander->logCache[i].state = EMPTY_STATE;
            llogHander->logCache[i].buffLen = 0;
        }
        GET_EMPTY_CACHE_FOR_WRITE_LOG();

		FIRST_READ_LOG_FILE(llogHander);
		//INIT_LOGINFO_SET_CACHE(llogHander->currentLogFile);
	    INIT_LOGINFO_SET_LOCK(llogHander->currentLogFile);
		
		return 0;
	} while(0);
	
	return 1;
}

int llog_init(const char *configFilePath)
{
    if(parse_ini(configFilePath, llog_get_config_fun, &(g_llog.configInfo))) {
        LLOG_ERR_PRINTF("Can't load %s\n", configFilePath);
        return 1;
    }

	if(llog_init_work_parameter(&g_llog)) {
		return 1;
	}

	CREATE_LLOG_WORK_THREAD(g_llog.currentLogFile.threadId, g_llog);
	
	return 0;
}

void llog_stop(void)
{
    g_llog.configInfo.logLevel = 99;
    g_llog.exitLogThread = LLOG_WORK_THREAD_STOP;
    pthread_join(g_llog.currentLogFile.threadId, NULL);
    return;
}

int llog_restart(LOG_ENUM logLevel)
{
    g_llog.configInfo.logLevel = logLevel;
	g_llog.exitLogThread = LLOG_WORK_THREAD_START;
	if(pthread_kill(g_llog.currentLogFile.threadId, 0) == ESRCH) {
		CREATE_LLOG_WORK_THREAD(g_llog.currentLogFile.threadId, g_llog);
	}
    return;
}

void test3(void)
{
	char *buf = "luoqiang test\n";

	int i,j;
	struct timeval start, end;

	printf("start...\n");
	gettimeofday(&start, NULL);
	for(i = 0; i < 100; ++i)
	{
		for(j = 0; j < 10000; ++j) {
            //printf("|%d-%d|\n", i, j);
			LLOG_WARN_PRO("%s", buf);
        }
        //printf("kkkkk=%d\n", i);
	}
	gettimeofday(&end, NULL);
	printf("end...\n");

	int us = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
	printf("use time:%0.4f(s)\n", us/1000000.0);
}

int main(int argc, char **argv)
{
#define TEST_INI_FILE_PATH "ini_test.ini"
	if(llog_init(TEST_INI_FILE_PATH)) {
        printf("failllllllllll\n");
        return 1;
    }
	test3();
	llog_stop();
	return 0;
}

