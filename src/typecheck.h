#ifndef kirby_typecheck_h
#define kirby_typecheck_h

#include <stdbool.h>

#include "ast.h"
#include "token.h"
#include "types.h"

typedef struct TypeEnv TypeEnv;

// Shared diagnostic reporting, also used by definite_assignment.c.
void typchkErrorAtToken(Token *token, const char *message);
void typchkErrorAtTokenFmt(Token *token, const char *fmt, ...);

void typchkSessionBegin(void);
void typchkSessionEnd(void);

TypeEnv *typchkTypeEnvCreate(void);
void typchkTypeEnvDestroy(TypeEnv *env);

// Local variable bindings

void typchkTypeEnvBeginScope(TypeEnv *env);
void typchkTypeEnvEndScope(TypeEnv *env);
void typchkTypeEnvDeclare(TypeEnv *env, Token name, Type *type);

// Return a Type pointer for the given name
//
// Returns NULL if not found in type env
Type *typchkTypeEnvLookup(TypeEnv *env, Token name);

void typchkTypeEnvRegisterStruct(TypeEnv *env, Token name, Type *type);
Type *typchkTypeEnvLookupStruct(TypeEnv *env, Token name);

void typchkTypeEnvRegisterFunction(TypeEnv *env, Token name, Type *type);
Type *typchkTypeEnvLookupFunction(TypeEnv *env, Token name);

// Resolve an AstNode to a Type pointer
//
// Returns NULL and reports a [line N] Error diagnostic
Type *typchkResolveType(TypeEnv *env, AstNode *typeAnnotation);

void typchkTypeEnvSetSelfType(TypeEnv *env, Type *selfType);
Type *typchkTypeEnvGetSelfType(TypeEnv *env);

void typchkTypeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType);
Type *typchkTypeEnvGetCurrentReturnType(TypeEnv *env);

Type *typchkInfer(TypeEnv *env, AstNode *node);
bool typchkCheck(TypeEnv *env, AstNode *node, Type *expected);

void typchkCheckStmt(TypeEnv *env, AstNode *node);

bool typchkCheckProgram(AstNode **program, int count);

bool typchkHadError(void);
void typchkResetError(void);

#endif
