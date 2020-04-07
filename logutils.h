#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>

// #define DEBUG // log开关

#ifdef DEBUG
#define LOGD(format, ...) printf("[%s:%d][%s]: " format "\n", __FILE__,__LINE__, __FUNCTION__,\
                             ##__VA_ARGS__)
#else
#define LOGD(format, ...)
#endif

#define LOGI(format, ...) {time_t now=time(NULL);tm* local = localtime(&now);char buf[128] = {0};\
	strftime(buf, 128,"%Y-%m-%d %H:%M:%S", local);printf("[%s] [%s:%d] [%s] " format "\n",buf, __FILE__,__LINE__, __FUNCTION__,##__VA_ARGS__);}
