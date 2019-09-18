#ifndef __LLOG__
#define __LLOG__

#ifdef __cplusplus
extern "C"{
#endif

#if 0
#define PRE_TAG "%04d-%02d-%02d %02d:%02d:%02d [%s:%s:%d] "
#define VAL_TAG ,pTM->tm_year+1900,pTM->tm_mon+1,pTM->tm_mday,pTM->tm_hour,pTM->tm_min,pTM->tm_sec,__FILE__,__FUNCTION__,__LINE__

#define LLOG_DEBUG_PRO(format, ...) ({\
	struct tm * pTM;\
    time_t timer = time(NULL);\
	pTM = localtime(&timer);\
	llog_debug(PRE_TAG format VAL_TAG, ##__VA_ARGS__);\
	})


#define PRE_TAG "[%s:%s:%d] "
#define VAL_TAG ,__FILE__,__FUNCTION__,__LINE__
#endif
typedef enum LOG_ENUM_E
{
	LOG_LEVEL_DEBUG = 0,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LOG_ENUM;

#define LLOG_DEBUG_PRO(format, ...) LLOG_FORMAT_PRO(LOG_LEVEL_DEBUG, "DEBUG", format, ##__VA_ARGS__)
#define LLOG_INFO_PRO(format, ...) LLOG_FORMAT_PRO(LOG_LEVEL_INFO, "INFO", format, ##__VA_ARGS__)
#define LLOG_WARN_PRO(format, ...) LLOG_FORMAT_PRO(LOG_LEVEL_WARN, "WARN", format, ##__VA_ARGS__)
#define LLOG_ERROR_PRO(format, ...) LLOG_FORMAT_PRO(LOG_LEVEL_ERROR, "ERROR", format, ##__VA_ARGS__)
#define LLOG_TRACE_PRO(format, ...) LLOG_FORMAT_PRO(LOG_LEVEL_FATAL, "TRACE", format, ##__VA_ARGS__)

#define LLOG_DEBUG(format, ...) LLOG_FORMAT(LOG_LEVEL_DEBUG, "DEBUG", format, ##__VA_ARGS__)
#define LLOG_INFO(format, ...) LLOG_FORMAT(LOG_LEVEL_WARN, "INFO", format, ##__VA_ARGS__)
#define LLOG_WARN(format, ...) LLOG_FORMAT(LOG_LEVEL_WARN, "WARN", format, ##__VA_ARGS__)
#define LLOG_ERROR(format, ...) LLOG_FORMAT(LOG_LEVEL_ERROR, "ERROR", format, ##__VA_ARGS__)
#define LLOG_TRACE(format, ...) LLOG_FORMAT(LOG_LEVEL_FATAL, "TRACE", format, ##__VA_ARGS__)

int llog_init(const char *configFilePath);
int llog_restart(LOG_ENUM logLevel);
void llog_stop(void);

#ifdef __cplusplus
}
#endif

#endif
