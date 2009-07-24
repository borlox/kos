#ifndef _STRING_H
#define _STRING_H

#include <sys/types.h>

void *memmove(void *dest, const void *src, size_t n);
void* memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void* memcpy(void *dest, const void *src, size_t n);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);

size_t strlen(const char *s);
char *strstr(const char *haystack, const char *needle);

char *strchr(const char *s, int c);

#endif
