#ifndef kirby_typenv_h
#define kirby_typenv_h

#include "token.h"
#include "types.h"

typedef struct TypeEnv TypeEnv;

TypeEnv *typeEnvInit(void);
void typeEnvFree(TypeEnv *env);

// Local variable bindings

void typeEnvBeginScope(TypeEnv *env);
void typeEnvEndScope(TypeEnv *env);
void typeEnvDeclare(TypeEnv *env, Token name, Type *type);

// Return a Type pointer for the given name
//
// Returns NULL if not found
Type *typeEnvLookupName(TypeEnv *env, Token name);

void typeEnvRegisterStruct(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupStruct(TypeEnv *env, Token name);

void typeEnvRegisterFunction(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupFunction(TypeEnv *env, Token name);

void typeEnvRegisterAlias(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupAlias(TypeEnv *env, Token name);

void typeEnvRegisterTrait(TypeEnv *env, Token name, Type *type);
Type *typeEnvLookupTrait(TypeEnv *env, Token name);
int typeEnvTraitCount(TypeEnv *env);

// WIP State

void typeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType);
Type *typeEnvGetCurrentReturnType(TypeEnv *env);

void typeEnvSetSelfType(TypeEnv *env, Type *selfType);
Type *typeEnvGetSelfType(TypeEnv *env);

void typeEnvSetImplTargetType(TypeEnv *env, Type *implTargetType);
Type *typeEnvGetImplTargetType(TypeEnv *env);

#endif
