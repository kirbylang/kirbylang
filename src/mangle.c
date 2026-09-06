#include <stdio.h>

#include "mangle.h"

int mangledPrimitiveMethodName(char *buffer, const char *primitiveTypeName,
                               int primitiveTypeNameLength,
                               const Token *traitName,
                               const Token *methodName) {
  int len;

  if (traitName != NULL) {
    len =
        snprintf(buffer, MANGLED_NAME_MAX, "@%.*s.%.*s.%.*s",
                 primitiveTypeNameLength, primitiveTypeName, traitName->length,
                 traitName->start, methodName->length, methodName->start);
  } else {
    len = snprintf(buffer, MANGLED_NAME_MAX, "@%.*s.%.*s",
                   primitiveTypeNameLength, primitiveTypeName,
                   methodName->length, methodName->start);
  }

  if (len < 0)
    return 0;
  if (len >= MANGLED_NAME_MAX)
    len = MANGLED_NAME_MAX - 1;
  return len;
}
