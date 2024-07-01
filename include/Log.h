#ifndef __LOG_H__
#define __LOG_H__ 

#include <cstdio>

#define LOG_ERR(fmt, ...) printf(fmt, ##__VA_ARGS__)

#endif /* __LOG_H__ */
