#ifndef kirby_typecheck_h
#define kirby_typecheck_h

#include <stdbool.h>

#include "ast.h"
#include "token.h"
#include "typenv.h"
#include "types.h"

// Shared diagnostic reporting, also used by definite_assignment.c.
void typchkErrorAtToken(Token *token, const char *message);
void typchkErrorAtTokenFmt(Token *token, const char *fmt, ...);

void typchkSessionBegin(void);
void typchkSessionEnd(void);

// Resolve an AstNode to a Type pointer
//
// Returns NULL and reports a [line N] Error diagnostic
Type *typchkResolveType(TypeEnv *env, AstNode *typeAnnotation);

// Infer the type of an AST node
Type *infer(TypeEnv *env, AstNode *node);

// Typecheck an AST node against an expected type
bool check(TypeEnv *env, AstNode *node, Type *expected);

void checkStmt(TypeEnv *env, AstNode *node);

bool typchkCheckProgram(AstNode **program, int count);

bool _hadTypecheckError(void);
void _resetHadTypecheckError(void);

#endif
