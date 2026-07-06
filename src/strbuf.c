#include "strbuf.h"

void sb_init(StrBuf *sb) {
  sb->capacity = 256;
  sb->len = 0;
  sb->data = (char *)malloc(sb->capacity);
  sb->data[0] = '\0';
}

void sb_ensure(StrBuf *sb, size_t extra) {
  if (sb->len + extra + 1 > sb->capacity) {
    while (sb->len + extra + 1 > sb->capacity) {
      sb->capacity *= 2;
    }

    sb->data = (char *)realloc(sb->data, sb->capacity);
  }
}

void sb_append(StrBuf *sb, const char *text) {
  size_t len = strlen(text);
  sb_ensure(sb, len);
  memcpy(sb->data + sb->len, text, len + 1);
  sb->len += len;
}

void sb_appendf(StrBuf *sb, const char *fmt, ...) {
  char tmp[256];

  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, args);
  va_end(args);

  sb_append(sb, tmp);
}

void sb_free(StrBuf *sb) { free(sb->data); }
