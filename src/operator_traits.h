#ifndef kirby_operator_traits_h
#define kirby_operator_traits_h

#include <stdbool.h>
#include <stddef.h>

#include "token.h"

/**
 * An operator that a type can support by implementing a trait, as in `a + b`
 * calling `a.add(b)`. The trait registry, the type checker and the compiler
 * all read this table.
 */
typedef struct {
  TokenType token;
  const char *trait;
  const char *method;
  bool isUnary;
  // Whether the trait is defined from this entry. Eq and Ord are not, because
  // they also carry the `==` and `<` family together.
  bool definesTrait;
} OperatorTrait;

static inline const OperatorTrait *operatorTraitAt(int index, int *count) {
  static const OperatorTrait table[] = {
      {TOKEN_PLUS, "Add", "add", false, true},
      {TOKEN_MINUS, "Sub", "sub", false, true},
      {TOKEN_STAR, "Mul", "mul", false, true},
      {TOKEN_SLASH, "Div", "div", false, true},
      {TOKEN_MODULO, "Mod", "mod", false, true},
      {TOKEN_MINUS, "Neg", "neg", true, true},
      {TOKEN_EQUAL_EQUAL, "Eq", "equals", false, false},
      {TOKEN_BANG_EQUAL, "Eq", "equals", false, false},
      {TOKEN_LESS, "Ord", "cmp", false, false},
      {TOKEN_GREATER, "Ord", "cmp", false, false},
      {TOKEN_LESS_EQUAL, "Ord", "cmp", false, false},
      {TOKEN_GREATER_EQUAL, "Ord", "cmp", false, false},
  };

  *count = (int)(sizeof(table) / sizeof(table[0]));
  return &table[index];
}

// The entry for a binary or unary operator token, or NULL if it has none.
static inline const OperatorTrait *operatorTraitFor(TokenType token,
                                                    bool isUnary) {
  int count;
  operatorTraitAt(0, &count);

  for (int i = 0; i < count; i++) {
    const OperatorTrait *entry = operatorTraitAt(i, &count);

    if (entry->token == token && entry->isUnary == isUnary)
      return entry;
  }

  return NULL;
}

#endif
