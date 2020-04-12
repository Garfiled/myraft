#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG
#define LOGD(format, ...) {time_t now=time(NULL);tm* local = localtime(&now);char buf[128] = {0};\
	strftime(buf, 128,"%Y-%m-%d %H:%M:%S", local);printf("[%s] [DEBUG] [%s:%d] [%s] " format "\n",buf, __FILE__,__LINE__, __FUNCTION__,##__VA_ARGS__);}
#else
#define LOGD(format, ...)
#endif

#define LOGI(format, ...) {time_t now=time(NULL);tm* local = localtime(&now);char buf[128] = {0};\
	strftime(buf, 128,"%Y-%m-%d %H:%M:%S", local);printf("[%s] [INFO] [%s:%d] [%s] " format "\n",buf, __FILE__,__LINE__, __FUNCTION__,##__VA_ARGS__);}

#define LOGE(format, ...) {time_t now=time(NULL);tm* local = localtime(&now);char buf[128] = {0};\
	strftime(buf, 128,"%Y-%m-%d %H:%M:%S", local);printf("[%s] [ERROR] [%s:%d] [%s] " format "\n",buf, __FILE__,__LINE__, __FUNCTION__,##__VA_ARGS__);}
