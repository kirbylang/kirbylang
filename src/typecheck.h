#ifndef kirby_typecheck_h
#define kirby_typecheck_h

#include <stdbool.h>

#include "ast.h"
#include "token.h"
#include "types.h"

typedef struct TypeEnv TypeEnv;

// Shared diagnostic reporting, also used by definite_assignment.c.
void errorAtToken(Token *token, const char *message);
void errorAtTokenFmt(Token *token, const char *fmt, ...);

TypeEnv *typeEnvCreate(void);
void typeEnvDestroy(TypeEnv *env);

// Local variable bindings

void typeEnvBeginScope(TypeEnv *env);
void typeEnvEndScope(TypeEnv *env);
void typeEnvDeclare(TypeEnv *env, Token name, Type *type);

// Return a Type pointer for the given name
//
// Returns NULL if not found in type env
Type *typeEnvLookup(TypeEnv *env, Token name);

void typeEnvRegisterStruct(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupStruct(TypeEnv *env, Token name);

void typeEnvRegisterFunction(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupFunction(TypeEnv *env, Token name);

// Resolve an AstNode to a Type pointer
//
// Returns NULL and reports a [line N] Error diagnostic
Type *resolveType(TypeEnv *env, AstNode *typeAnnotation);

void typeEnvSetSelfType(TypeEnv *env, Type *selfType);
Type *typeEnvGetSelfType(TypeEnv *env);

void typeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType);
Type *typeEnvGetCurrentReturnType(TypeEnv *env);

Type *infer(TypeEnv *env, AstNode *node);
bool check(TypeEnv *env, AstNode *node, Type *expected);

void checkStmt(TypeEnv *env, AstNode *node);

bool typecheckProgram(AstNode **program, int count);

bool typecheckHadError(void);
void typecheckResetError(void);

#endif
