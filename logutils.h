#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEBUG // log开关
 
#define __FILENAME__ (strrchr(__FILE__, '/') + 1) // 文件名

#ifdef DEBUG
#define LOGD(format, ...) printf("[%s:%d][%s]: " format "\n", __FILE__,__LINE__, __FUNCTION__,\
                             ##__VA_ARGS__)
#else
#define LOGD(format, ...)
#endif
