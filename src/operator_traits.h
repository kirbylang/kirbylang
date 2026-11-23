#ifndef kirby_operator_traits_h
#define kirby_operator_traits_h

#include <stdbool.h>
#include <stddef.h>

#include "token.h"

/**
 * An operator that a type supports by implementing a trait, as in `a + b`
 * calling `a.add(b)`. The type checker and the compiler read this table to
 * find the trait and method for an operator. The traits themselves are defined
 * in `typchkTypeEnvDefineBuiltinTraits`, and a unit test checks that every row
 * here matches them.
 */
typedef struct {
  TokenType token;
  const char *trait;
  const char *method;
  bool isUnary;
} OperatorTrait;

static const OperatorTrait table[] = {
    {TOKEN_PLUS, "Add", "add", false},
    {TOKEN_MINUS, "Sub", "sub", false},
    {TOKEN_STAR, "Mul", "mul", false},
    {TOKEN_SLASH, "Div", "div", false},
    {TOKEN_MODULO, "Mod", "mod", false},
    {TOKEN_MINUS, "Neg", "neg", true},
    {TOKEN_EQUAL_EQUAL, "Eq", "equals", false},
    {TOKEN_BANG_EQUAL, "Eq", "equals", false},
    {TOKEN_LESS, "Ord", "cmp", false},
    {TOKEN_GREATER, "Ord", "cmp", false},
    {TOKEN_LESS_EQUAL, "Ord", "cmp", false},
    {TOKEN_GREATER_EQUAL, "Ord", "cmp", false},
};

static inline const int getOperatorTraitCount() {
  return (int)(sizeof(table) / sizeof(table[0]));
}

// The entry for a binary or unary operator token, or NULL if it has none.
static inline const OperatorTrait *operatorTraitFor(TokenType token,
                                                    bool isUnary) {
  int count = getOperatorTraitCount();
  for (int i = 0; i < count; i++) {
    if (table[i].token == token && table[i].isUnary == isUnary)
      return &table[i];
  }

  return NULL;
}

#endif
