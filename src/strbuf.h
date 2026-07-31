#ifndef kirby_strbuf_h
#define kirby_strbuf_h

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len;
  size_t capacity;
} StrBuf;

void sb_init(StrBuf *sb);
void sb_ensure(StrBuf *sb, size_t extra);
void sb_append(StrBuf *sb, const char *text);
void sb_appendf(StrBuf *sb, const char *fmt, ...);
void sb_free(StrBuf *sb);

#endif
