#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definite_assignment.h"
#include "resolved_impl_targets.h"
#include "typecheck.h"

static bool hadError = false;

static bool _tokensEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static bool _tokenTextEquals(Token *token, const char *text) {
  size_t len = strlen(text);
  if ((size_t)token->length != len)
    return false;
  return memcmp(token->start, text, len) == 0;
}

static bool _tokenIsPrimitiveTypeName(Token *token) {
  return _tokenTextEquals(token, "unit") || _tokenTextEquals(token, "bool") ||
         _tokenTextEquals(token, "string") || _tokenTextEquals(token, "f64") ||
         _tokenTextEquals(token, "Array");
}

static Token _makeTokenFromCString(const char *text) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0;
  return token;
}

void typchkErrorAtToken(Token *token, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

void typchkErrorAtTokenFmt(Token *token, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  typchkErrorAtToken(token, message);
}

static void _errorAtNode(AstNode *node, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error: %s\n", node->line, message);
}

static void _errorAtNodeFmt(AstNode *node, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  _errorAtNode(node, message);
}

bool _hadTypecheckError(void) { return hadError; }
void _resetHadTypecheckError(void) { hadError = false; }

// Type environment that persists across compilation units
static TypeEnv *sessionEnv = NULL;

void typchkSessionBegin(void) {
  if (sessionEnv != NULL)
    return;
  sessionEnv = typeEnvInit();
  typeEnvBeginScope(sessionEnv);
}

void typchkSessionEnd(void) {
  if (sessionEnv == NULL)
    return;

  typeEnvEndScope(sessionEnv);
  typeEnvFree(sessionEnv);

  sessionEnv = NULL;

  typesFreeAll();
}

Type *typchkResolveType(TypeEnv *env, AstNode *typeAnnotation) {
  if (typeAnnotation->kind == NODE_TYPE_FUNCTION) {
    TypeFunctionNode *fn = &typeAnnotation->as.typeFunction;

    Type **paramTypes = NULL;
    if (fn->paramCount > 0) {
      paramTypes = (Type **)typesAllocRaw(fn->paramCount * sizeof(Type *));
      for (int i = 0; i < fn->paramCount; i++) {
        Type *paramType = typchkResolveType(env, fn->paramTypes[i]);
        if (paramType == NULL)
          return NULL; // error already reported below the recursive call
        paramTypes[i] = paramType;
      }
    }

    Type *returnType = typchkResolveType(env, fn->returnType);
    if (returnType == NULL)
      return NULL;

    return typeFunction(paramTypes, fn->paramCount, returnType);
  }

  TypeNode *t = &typeAnnotation->as.type_;

  if (t->genericArgCount > 0) {
    typchkErrorAtToken(&t->name, "Generic types aren't supported yet.");
    return NULL;
  }

  if (_tokenTextEquals(&t->name, "unit"))
    return typeUnit();
  if (_tokenTextEquals(&t->name, "bool"))
    return typeBool();
  if (_tokenTextEquals(&t->name, "string"))
    return typeString();
  if (_tokenTextEquals(&t->name, "f64"))
    return typeF64();
  if (_tokenTextEquals(&t->name, "Array"))
    return typeArray(NULL);
  if (_tokenTextEquals(&t->name, "Self")) {
    return typeEnvGetImplTargetType(env) != NULL ? typeEnvGetImplTargetType(env)
                                                 : typeSelfPlaceholder();
  }

  Type *structType = typeEnvLookupStruct(env, t->name);
  if (structType != NULL)
    return structType;

  Type *aliasType = typeEnvLookupAlias(env, t->name);
  if (aliasType != NULL)
    return aliasType;

  typchkErrorAtToken(&t->name, "Unknown type.");
  return NULL;
}

static Type *_inferLiteral(AstNode *node);
static Type *_inferUnary(TypeEnv *env, AstNode *node);
static Type *_inferBinary(TypeEnv *env, AstNode *node);
static Type *_inferVariable(TypeEnv *env, AstNode *node);
static Type *_inferAssign(TypeEnv *env, AstNode *node);
static Type *_inferLogical(TypeEnv *env, AstNode *node);
static Type *_inferNullish(TypeEnv *env, AstNode *node);
static Type *_inferCall(TypeEnv *env, AstNode *node);
static Type *_checkCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                           Type *calleeType);
static Type *_inferGet(TypeEnv *env, AstNode *node);
static Type *_inferSet(TypeEnv *env, AstNode *node);
static Type *_inferSelf(TypeEnv *env, AstNode *node);
static Type *_inferIndexGet(TypeEnv *env, AstNode *node);
static Type *_inferIndexSet(TypeEnv *env, AstNode *node);
static Type *_inferStructInit(TypeEnv *env, AstNode *node);
static Type *_inferArray(TypeEnv *env, AstNode *node);
static Type *_inferIf(TypeEnv *env, AstNode *node);
static Type *_inferBlockExpr(TypeEnv *env, AstNode *node);
static Type *_checkBlockContents(TypeEnv *env, BlockNode *block,
                                 Type *expectedValueType);
static Type *_checkOrInferLambda(TypeEnv *env, AstNode *node, Type *expected);
static void _checkVarDecl(TypeEnv *env, AstNode *node);
static void _checkFunctionDecl(TypeEnv *env, AstNode *node);

// Infer the type of an expression
Type *infer(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_LITERAL:
    return _inferLiteral(node);
  case NODE_UNARY:
    return _inferUnary(env, node);
  case NODE_BINARY:
    return _inferBinary(env, node);
  case NODE_GROUPING:
    return infer(env, node->as.grouping.inner);
  case NODE_VARIABLE:
    return _inferVariable(env, node);
  case NODE_ASSIGN:
    return _inferAssign(env, node);
  case NODE_AND:
  case NODE_OR:
    return _inferLogical(env, node);
  case NODE_NULLISH:
    return _inferNullish(env, node);
  case NODE_CALL:
    return _inferCall(env, node);
  case NODE_GET:
    return _inferGet(env, node);
  case NODE_SET:
    return _inferSet(env, node);
  case NODE_SELF:
    return _inferSelf(env, node);
  case NODE_INDEX_GET:
    return _inferIndexGet(env, node);
  case NODE_INDEX_SET:
    return _inferIndexSet(env, node);
  case NODE_STRUCT_INIT:
    return _inferStructInit(env, node);
  case NODE_ARRAY:
    return _inferArray(env, node);
  case NODE_IF:
    return _inferIf(env, node);
  case NODE_BLOCK:
    return _inferBlockExpr(env, node);
  case NODE_FUNCTION:
    if (node->as.function.isLambda)
      return _checkOrInferLambda(env, node, NULL);
    _errorAtNode(node, "Internal: unexpected function declaration in "
                       "expression position.");
    return NULL;
  default:
    _errorAtNode(node, "Internal: this isn't a checkable expression.");
    return NULL;
  }
}

bool check(TypeEnv *env, AstNode *node, Type *expected) {
  if (node->kind == NODE_FUNCTION && node->as.function.isLambda)
    return _checkOrInferLambda(env, node, expected) != NULL;

  if (node->kind == NODE_ARRAY && expected != NULL) {
    if (expected->kind != TYPE_ARRAY) {
      _errorAtNodeFmt(node, "Expected %s, got an array.",
                      typeToString(expected));
      return false;
    }

    ArrayNode *arr = &node->as.array;

    bool ok = true;

    for (int i = 0; i < arr->count; i++) {
      AstNode *item = arr->items[i];

      if (!check(env, item, expected->as.array.elementType))
        ok = false;
    }

    return ok;
  }

  Type *actual = infer(env, node);

  if (actual == NULL)
    return true;

  if (expected == NULL)
    return true;

  if (!typesEqual(actual, expected)) {
    _errorAtNodeFmt(node, "Expected %s, got %s.", typeToString(expected),
                    typeToString(actual));
    return false;
  }

  return true;
}

// True if either: struct is generic (unsupported currently) or if struct
// members couldn't be type checked
static bool _areStructMembersUnreliable(Type *type) {
  return typeStructIsGeneric(type) || typeStructHasUnresolvedMembers(type);
}

// Infer the type of a literal value expression
static Type *_inferLiteral(AstNode *node) {
  LiteralNode *lit = &node->as.literal;
  switch (lit->kind) {
  case LITERAL_NIL:
  case LITERAL_UNIT:
    return typeUnit();
  case LITERAL_BOOL:
    return typeBool();
  case LITERAL_NUMBER:
    return typeF64();
  case LITERAL_STRING:
    return typeString();
  }
  return NULL; // unreachable
}

// Infer the type of a unary expressoin
static Type *_inferUnary(TypeEnv *env, AstNode *node) {
  UnaryNode *u = &node->as.unary;

  if (u->op.type == TOKEN_BANG) {
    Type *operandType = infer(env, u->operand);

    if (operandType == NULL)
      return NULL;

    return typeBool();
  }

  if (!check(env, u->operand, typeF64()))
    return NULL;

  return typeF64();
}

// Infer the type of a binary expression
static Type *_inferBinary(TypeEnv *env, AstNode *node) {
  BinaryNode *b = &node->as.binary;
  Type *leftType = infer(env, b->left);
  Type *rightType = infer(env, b->right);

  if (leftType == NULL || rightType == NULL)
    return NULL;

  switch (b->op.type) {
  case TOKEN_PLUS:
    if (typesEqual(leftType, typeF64()) && typesEqual(rightType, typeF64()))
      return typeF64();

    if (typesEqual(leftType, typeString()) &&
        typesEqual(rightType, typeString()))
      return typeString();

    typchkErrorAtTokenFmt(&b->op,
                          "'+' needs two f64s or two strings, got %s and %s.",
                          typeToString(leftType), typeToString(rightType));
    return NULL;

  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MODULO:
    if (!typesEqual(leftType, typeF64())) {
      _errorAtNodeFmt(b->left, "Expected f64, got %s.", typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      _errorAtNodeFmt(b->right, "Expected f64, got %s.",
                      typeToString(rightType));
      return NULL;
    }
    return typeF64();

  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
    if (!typesEqual(leftType, rightType)) {
      typchkErrorAtTokenFmt(&b->op,
                            "Both sides of '%s' must be the same type, got %s "
                            "and %s.",
                            b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=",
                            typeToString(leftType), typeToString(rightType));
      return NULL;
    }
    if (leftType->kind == TYPE_STRUCT &&
        !typeStructImplementsTrait(
            leftType, internTokenName(_makeTokenFromCString("Eq")))) {
      typchkErrorAtTokenFmt(&b->op,
                            "%s needs 'impl Eq for %s' to support '%s'.",
                            typeToString(leftType), typeToString(leftType),
                            b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=");
      return NULL;
    }
    return typeBool();

  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    if (!typesEqual(leftType, typeF64())) {
      _errorAtNodeFmt(b->left, "Expected f64, got %s.", typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      _errorAtNodeFmt(b->right, "Expected f64, got %s.",
                      typeToString(rightType));
      return NULL;
    }
    return typeBool();

  default:
    typchkErrorAtToken(&b->op, "Internal: unhandled binary operator.");
    return NULL;
  }
}

// Infer the type of an variable/identifier expression
static Type *_inferVariable(TypeEnv *env, AstNode *node) {
  Token *name = &node->as.variable.name;
  Type *type = typeEnvLookupName(env, *name);

  if (type == NULL)
    type = typeEnvLookupFunction(env, *name);

  if (type == NULL)
    return NULL;

  return type;
}

// Infer the type of an assignment expression
static Type *_inferAssign(TypeEnv *env, AstNode *node) {
  AssignNode *a = &node->as.assign;
  Type *varType = typeEnvLookupName(env, a->name);

  if (varType == NULL)
    varType = typeEnvLookupFunction(env, a->name);

  if (varType == NULL) {
    infer(env, a->value);
    return NULL;
  }

  if (!check(env, a->value, varType))
    return NULL;

  return varType;
}

// Infer the type of a logic operator e.g. and, or
static Type *_inferLogical(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical;

  // Both operands are conditions, so both are bool and so is the result.
  // The VM still short circuits, it just can't yield a non-bool operand.
  bool ok = check(env, l->left, typeBool());

  if (!check(env, l->right, typeBool()))
    ok = false;

  return ok ? typeBool() : NULL;
}

// Infer the type of a nullish expression
static Type *_inferNullish(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical; // reuses LogicalNode, like compiler.c
  infer(env,
        l->left); // unconstrained -- no Option[T] to check against yet
  return infer(env, l->right); // result type comes from the fallback
}

// Reads a constant number out of an expression: a number literal, possibly
// negated with '-' and possibly wrapped in parentheses. Anything else, such as
// a variable or arithmetic, isn't treated as constant.
static bool _getConstantNumberFromExpr(AstNode *node, double *value) {
  switch (node->kind) {
  case NODE_LITERAL:
    if (node->as.literal.kind != LITERAL_NUMBER)
      return false;
    *value = node->as.literal.as.number;
    return true;
  case NODE_GROUPING:
    return _getConstantNumberFromExpr(node->as.grouping.inner, value);
  case NODE_UNARY:
    if (node->as.unary.op.type != TOKEN_MINUS)
      return false;
    if (!_getConstantNumberFromExpr(node->as.unary.operand, value))
      return false;
    *value = -*value;
    return true;
  default:
    return false;
  }
}

// Some natives can never accept certain arguments. When the argument is a
// constant, report it now instead of when the call runs. Arguments that aren't
// constant are still checked at runtime by the native itself.
static void _checkNativeConstantArgs(AstNode *node) {
  CallNode *c = &node->as.call;

  if (c->callee->kind != NODE_VARIABLE || c->argCount != 1)
    return;

  double value;

  if (_tokenTextEquals(&c->callee->as.variable.name, "@sqrt") &&
      _getConstantNumberFromExpr(c->args[0], &value) && value < 0) {
    _errorAtNodeFmt(c->args[0],
                    "function @sqrt expects argument 1 to be a "
                    "non-negative number but got %g.",
                    value);
  }
}

// Infer the type of a call expression
static Type *_inferCall(TypeEnv *env, AstNode *node) {
  CallNode *c = &node->as.call;

  if (c->callee->kind == NODE_VARIABLE) {
    Token *name = &c->callee->as.variable.name;
    Type *calleeType = typeEnvLookupName(env, *name);

    if (calleeType == NULL)
      calleeType = typeEnvLookupFunction(env, *name);

    if (calleeType == NULL) {
      // Unable to resolve called function (possible a native function)
      // Run infer over args to report any type errors they might contain
      for (int i = 0; i < c->argCount; i++)
        infer(env, c->args[i]);

      return NULL;
    }

    Type *returnType = _checkCallAgainstFunctionType(env, node, calleeType);

    if (returnType != NULL)
      // Used to validate args passed native functions like `@sqrt`
      _checkNativeConstantArgs(node);

    return returnType;
  }

  Type *calleeType = infer(env, c->callee);

  if (calleeType == NULL)
    return NULL;

  return _checkCallAgainstFunctionType(env, node, calleeType);
}

static Type *_checkCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                           Type *calleeType) {
  CallNode *c = &node->as.call;

  if (calleeType->kind != TYPE_FN) {
    typchkErrorAtTokenFmt(&c->paren, "%s isn't callable.",
                          typeToString(calleeType));
    return NULL;
  }
  if (c->argCount != calleeType->as.function.paramCount) {
    typchkErrorAtTokenFmt(&c->paren, "Expected %d argument(s), got %d.",
                          calleeType->as.function.paramCount, c->argCount);
    return NULL;
  }

  bool ok = true;
  for (int i = 0; i < c->argCount; i++) {
    if (!check(env, c->args[i], calleeType->as.function.paramTypes[i]))
      ok = false;
  }
  if (!ok)
    return NULL;

  return calleeType->as.function.returnType;
}

// Infer the type of a get expression
static Type *_inferGet(TypeEnv *env, AstNode *node) {
  GetNode *get = &node->as.get;

  if (get->object->kind == NODE_VARIABLE) {
    Token *objIdentifier = &get->object->as.variable.name;

    bool shadowed = typeEnvLookupName(env, *objIdentifier) != NULL ||
                    typeEnvLookupFunction(env, *objIdentifier) != NULL;

    if (!shadowed) {
      bool isSelf = _tokenTextEquals(objIdentifier, "Self");
      Type *structType = isSelf ? typeEnvGetImplTargetType(env)
                                : typeEnvLookupStruct(env, *objIdentifier);

      if (structType == NULL && isSelf) {
        typchkErrorAtTokenFmt(objIdentifier,
                              "'Self' can only be used inside an impl block.");
        return NULL;
      }

      if (structType != NULL) {
        if (_areStructMembersUnreliable(structType))
          return NULL;

        Type *methodType = typeStructStaticMethodLookup(structType, get->name);
        bool isOwnMethod = methodType != NULL;

        if (methodType == NULL) {
          methodType = typeStructTraitStaticMethodLookup(structType, get->name);
        }

        if (methodType == NULL) {
          typchkErrorAtTokenFmt(&get->name, "%s has no static method '%.*s'.",
                                typeToString(structType), get->name.length,
                                get->name.start);
          return NULL;
        }

        // Trait methods are always public; only an own (plain-impl)
        // method can be private.
        bool isPrivate = !typeStructStaticMethodIsPublic(structType, get->name);

        if (isOwnMethod && isPrivate &&
            typeEnvGetImplTargetType(env) != structType) {
          typchkErrorAtTokenFmt(&get->name, "Method '%.*s' is private to '%s'.",
                                get->name.length, get->name.start,
                                typeToString(structType));
          return NULL;
        }

        return methodType;
      }
    }
  }

  // Attempt to infer the object's type
  Type *objectType = infer(env, get->object);

  // No type found, bail
  if (objectType == NULL)
    return NULL;

  // The object's type isn't a struct
  if (objectType->kind != TYPE_STRUCT) {
    typchkErrorAtTokenFmt(&get->name, "Can't access '.%.*s' on a %s.",
                          get->name.length, get->name.start,
                          typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (_areStructMembersUnreliable(objectType))
    return NULL; // already reported once

  // Look up struct fields first
  Type *fieldType = typeStructFieldLookup(objectType, get->name);
  if (fieldType != NULL)
    return fieldType;

  // Then look up struct methods
  Type *methodType = typeStructInstanceMethodLookup(objectType, get->name);
  if (methodType != NULL) {
    if (!typeStructInstanceMethodIsPublic(objectType, get->name) &&
        typeEnvGetImplTargetType(env) != objectType) {
      typchkErrorAtTokenFmt(&get->name, "Method '%.*s' is private to '%s'.",
                            get->name.length, get->name.start,
                            typeToString(objectType));
      return NULL;
    }
    return methodType;
  }

  // Then look up methods that came from an `impl Trait for X` block
  Type *traitMethodType =
      typeStructTraitInstanceMethodLookup(objectType, get->name);
  if (traitMethodType != NULL)
    return traitMethodType;

  // Then check for accidental static method access
  Type *staticMethodType = typeStructStaticMethodLookup(objectType, get->name);
  if (staticMethodType == NULL) {
    staticMethodType = typeStructTraitStaticMethodLookup(objectType, get->name);
  }
  if (staticMethodType != NULL) {
    typchkErrorAtTokenFmt(&get->name,
                          "'%.*s' is a static method. Access it on '%.*s' "
                          "instead of an instance.",
                          get->name.length, get->name.start,
                          objectType->as.struct_.name.length,
                          internedNameChars(objectType->as.struct_.name));
    return NULL;
  }

  // Report error if nothing was found
  typchkErrorAtTokenFmt(&get->name, "%s has no field or method '%.*s'.",
                        typeToString(objectType), get->name.length,
                        get->name.start);
  return NULL;
}

// Infer the type of a set expression
static Type *_inferSet(TypeEnv *env, AstNode *node) {
  SetNode *set = &node->as.set;
  Type *objectType = infer(env, set->object);

  if (objectType == NULL) {
    infer(env, set->value); // still walk for internal errors
    return NULL;
  }

  // Check attempts to set fields on a none struct
  if (objectType->kind != TYPE_STRUCT) {
    typchkErrorAtTokenFmt(&set->name, "Can't set '.%.*s' on a %s.",
                          set->name.length, set->name.start,
                          typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (_areStructMembersUnreliable(objectType)) {
    infer(env, set->value); // still walk for internal errors
    return NULL;
  }

  // Check struct for field
  Type *fieldType = typeStructFieldLookup(objectType, set->name);

  // Struct has no field by name
  if (fieldType == NULL) {
    typchkErrorAtTokenFmt(&set->name, "%s has no field '%.*s'.",
                          typeToString(objectType), set->name.length,
                          set->name.start);
    return NULL;
  }

  // Type check value being set matches struct field's type
  if (!check(env, set->value, fieldType))
    return NULL;

  return fieldType;
}

// Infer the type of the self parameter of a method
static Type *_inferSelf(TypeEnv *env, AstNode *node) {
  // Returns null if not inside a method
  Type *selfType = typeEnvGetSelfType(env);

  if (selfType == NULL) {
    _errorAtNode(node, "'self' isn't valid here.");
    return NULL;
  }

  return selfType;
}

// Infer the type of an index access get expression
static Type *_inferIndexGet(TypeEnv *env, AstNode *node) {
  IndexGetNode *indexGet = &node->as.indexGet;
  Type *objectType = infer(env, indexGet->object);

  if (objectType == NULL) {
    infer(env, indexGet->index);
    return NULL;
  }

  if (objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&indexGet->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!check(env, indexGet->index, typeF64()))
    return NULL;

  return objectType->as.array.elementType;
}

// Infer the type of an index access set expression
static Type *_inferIndexSet(TypeEnv *env, AstNode *node) {
  IndexSetNode *indexSet = &node->as.indexSet;
  Type *objectType = infer(env, indexSet->object);

  if (objectType == NULL) {
    infer(env, indexSet->index);
    infer(env, indexSet->value);
    return NULL;
  }

  if (objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&indexSet->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!check(env, indexSet->index, typeF64()))
    return NULL;

  Type *elementType = objectType->as.array.elementType;
  if (elementType != NULL) {
    if (!check(env, indexSet->value, elementType))
      return NULL;
  } else {
    infer(env, indexSet->value); // nothing to check against yet (empty-array
                                 // case), still walk for internal errors
  }

  return elementType;
}

// Infer the type of a struct initialization expression
static Type *_inferStructInit(TypeEnv *env, AstNode *node) {
  StructInitNode *structInit = &node->as.structInit;

  Type *structType;
  if (_tokenTextEquals(&structInit->name, "Self")) {
    structType = typeEnvGetImplTargetType(env);
    if (structType == NULL) {
      typchkErrorAtTokenFmt(&structInit->name,
                            "'Self' can only be used inside an impl block.");
      return NULL;
    }
  } else {
    structType = typeEnvLookupStruct(env, structInit->name);
  }

  if (structType == NULL) {
    typchkErrorAtTokenFmt(&structInit->name, "Unknown struct '%.*s'.",
                          structInit->name.length, structInit->name.start);
    return NULL;
  }

  if (_areStructMembersUnreliable(structType)) {
    for (int i = 0; i < structInit->fieldCount; i++) {
      infer(env, structInit->fields[i].value); // still walk for internal errors
    }
    return structType; // already reported once
  }

  // Missing required fields are a separate runtime-level check, not
  // this pass's concern.
  bool ok = true;
  for (int i = 0; i < structInit->fieldCount; i++) {
    StructInitFieldNode *field = &structInit->fields[i];
    Type *fieldType = typeStructFieldLookup(structType, field->name);
    if (fieldType == NULL) {
      typchkErrorAtTokenFmt(&field->name, "%s has no field '%.*s'.",
                            typeToString(structType), field->name.length,
                            field->name.start);
      ok = false;
      continue;
    }
    if (!check(env, field->value, fieldType))
      ok = false;
  }

  if (!ok)
    return NULL;

  return structType;
}

static Type *_inferArray(TypeEnv *env, AstNode *node) {
  ArrayNode *a = &node->as.array;
  if (a->count == 0)
    return typeArray(NULL); // nothing to learn an element type from yet

  Type *elementType = infer(env, a->items[0]);
  bool ok = true;
  for (int i = 1; i < a->count; i++) {
    if (elementType != NULL) {
      if (!check(env, a->items[i], elementType))
        ok = false;
    } else {
      infer(env, a->items[i]); // still walk for internal errors
    }
  }
  if (!ok)
    return NULL;
  return typeArray(elementType);
}

// Infer the type of an if expression
static Type *_inferIf(TypeEnv *env, AstNode *node) {
  IfNode *i = &node->as.if_;
  check(env, i->condition,
        typeBool()); // reported if wrong; still proceed

  Type *thenType = infer(env, i->thenBranch);
  Type *elseType =
      i->elseBranch != NULL ? infer(env, i->elseBranch) : typeUnit();

  if (thenType == NULL || elseType == NULL)
    return NULL;

  if (!typesEqual(thenType, elseType)) {
    _errorAtNodeFmt(node,
                    "if/else branches must produce the same type, got %s "
                    "and %s.",
                    typeToString(thenType), typeToString(elseType));
    return NULL;
  }

  return thenType;
}

static Type *_checkBlockContents(TypeEnv *env, BlockNode *block,
                                 Type *expectedValueType) {
  typeEnvBeginScope(env);

  for (int i = 0; i < block->count; i++) {
    checkStmt(env, block->stmts[i]);
  }

  Type *result;
  if (block->value != NULL) {
    if (expectedValueType != NULL) {
      result = check(env, block->value, expectedValueType) ? expectedValueType
                                                           : NULL;
    } else {
      result = infer(env, block->value);
    }
  } else {
    // No trailing value -- always unit. A declared return type can
    // still be satisfied via explicit `return`s; verifying every path
    // does so isn't attempted here.
    result = typeUnit();
  }

  typeEnvEndScope(env);
  return result;
}

// Infer the type of a block expression
static Type *_inferBlockExpr(TypeEnv *env, AstNode *node) {
  return _checkBlockContents(env, &node->as.block,
                             /*expectedValueType=*/NULL);
}

static bool _checkIfBlockAlwaysReturns(BlockNode *block);

// Check if the statement (AstNode) exits it's enclosing function
static bool _checkIfAlwaysReturns(AstNode *node) {
  if (node == NULL)
    return false;

  switch (node->kind) {
  case NODE_RETURN:
    return true;

  case NODE_BLOCK:
    return _checkIfBlockAlwaysReturns(&node->as.block);

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    return if_->elseBranch != NULL && _checkIfAlwaysReturns(if_->thenBranch) &&
           _checkIfAlwaysReturns(if_->elseBranch);
  }

  default:
    return false;
  }
}

static bool _checkIfBlockAlwaysReturns(BlockNode *block) {
  // Implicit return
  if (block->value != NULL)
    return true;

  for (int i = 0; i < block->count; i++) {
    if (_checkIfAlwaysReturns(block->stmts[i]))
      return true;
  }

  return false;
}

static bool _checkIfBodyProducesDeclaredValue(FunctionNode *fn,
                                              Type *returnType) {
  if (returnType == NULL || typesEqual(returnType, typeUnit()))
    return true;

  if (fn->exprBody != NULL)
    return true;

  return _checkIfBlockAlwaysReturns(&fn->body);
}

static Type *_checkOrInferLambda(
    TypeEnv *env, AstNode *node,
    Type *expected // expected is NULL in infer() context (every param
                   // needs an explicit type) or a TYPE_FN in check()
                   // context (untyped params take their type from the matching
                   // position). Shared by infer()'s NODE_FUNCTION case
                   // and check()'s lambda special case.
) {
  FunctionNode *fn = &node->as.function;

  if (expected != NULL && expected->kind != TYPE_FN) {
    _errorAtNodeFmt(node, "Expected %s here, not a function.",
                    typeToString(expected));
    return NULL;
  }

  if (expected != NULL && expected->as.function.paramCount != fn->arity) {
    _errorAtNodeFmt(node,
                    "Expected a function taking %d argument(s), this one "
                    "takes %d.",
                    expected->as.function.paramCount, fn->arity);
    return NULL;
  }

  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;

  bool ok = true;

  for (int i = 0; i < fn->arity; i++) {
    if (fn->paramTypes != NULL && fn->paramTypes[i] != NULL) {
      paramTypes[i] = typchkResolveType(env, fn->paramTypes[i]);
      if (paramTypes[i] == NULL)
        ok = false;
    } else if (expected != NULL) {
      paramTypes[i] = expected->as.function.paramTypes[i];
    } else {
      _errorAtNode(node,
                   "Can't infer this lambda's parameter types without more "
                   "context -- add explicit types, or use it somewhere "
                   "its type is already known.");
      paramTypes[i] = NULL;
      ok = false;
    }
  }

  if (!ok)
    return NULL;

  Type *declaredReturnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;
  Type *targetReturnType =
      declaredReturnType != NULL
          ? declaredReturnType
          : (expected != NULL ? expected->as.function.returnType : NULL);

  typeEnvBeginScope(env);

  for (int i = 0; i < fn->arity; i++) {
    typeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousReturnType = typeEnvGetCurrentReturnType(env);
  typeEnvSetCurrentReturnType(env, targetReturnType);

  Type *bodyResultType;
  if (fn->exprBody != NULL) {
    bodyResultType =
        targetReturnType != NULL
            ? (check(env, fn->exprBody, targetReturnType) ? targetReturnType
                                                          : NULL)
            : infer(env, fn->exprBody);
  } else {
    bodyResultType = _checkBlockContents(env, &fn->body, targetReturnType);
  }

  typeEnvSetCurrentReturnType(env, previousReturnType);
  typeEnvEndScope(env);

  if (!_checkIfBodyProducesDeclaredValue(fn, targetReturnType)) {
    _errorAtNodeFmt(node, "This lambda must return %s on every path.",
                    typeToString(targetReturnType));
    return NULL;
  }

  if (bodyResultType == NULL)
    return NULL;

  Type *actualReturnType =
      declaredReturnType != NULL ? declaredReturnType : bodyResultType;

  return typeFunction(paramTypes, fn->arity, actualReturnType);
}

static void _checkVarDecl(TypeEnv *env, AstNode *node) {
  VarDeclNode *varDecl = &node->as.varDecl;
  bool hasExpectedType = varDecl->declaredType != NULL;
  Type *declaredType =
      hasExpectedType ? typchkResolveType(env, varDecl->declaredType) : NULL;

  if (hasExpectedType) {
    Token declaredTypeIdentifier = varDecl->declaredType->as.type_.name;
    bool isSelf = _tokenTextEquals(&declaredTypeIdentifier, "Self");

    if (isSelf && typeEnvGetImplTargetType(env) == NULL) {
      typchkErrorAtTokenFmt(&declaredTypeIdentifier,
                            "'Self' can only be used inside an impl block.");
      return;
    }
  }

  if (varDecl->initializer != NULL) {
    if (declaredType != NULL) {
      check(env, varDecl->initializer, declaredType);
      typeEnvDeclare(env, varDecl->name, declaredType);
    } else {
      Type *inferred = infer(env, varDecl->initializer);
      typeEnvDeclare(env, varDecl->name, inferred);
    }
  } else {
    if (declaredType == NULL) {
      typchkErrorAtTokenFmt(&varDecl->name,
                            "'%.*s' needs a type -- it has no initializer to "
                            "infer one from.",
                            varDecl->name.length, varDecl->name.start);
    }

    typeEnvDeclare(env, varDecl->name, declaredType);
  }
}

// Checks a function/method body against an already-resolved signature.
// Callers handle registration differently (local fn vs. hoisted
// top-level/impl method), so that's not redone here.
static void _checkFunctionBody(TypeEnv *env, FunctionNode *fn,
                               Type **paramTypes, Type *returnType,
                               Type *selfType) {
  typeEnvBeginScope(env);
  for (int i = 0; i < fn->arity; i++) {
    typeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousSelfType = typeEnvGetSelfType(env);
  typeEnvSetSelfType(env, selfType);

  Type *previousReturnType = typeEnvGetCurrentReturnType(env);
  typeEnvSetCurrentReturnType(env, returnType);

  if (fn->exprBody != NULL) {
    if (returnType != NULL)
      check(env, fn->exprBody, returnType);
    else
      infer(env, fn->exprBody);
  } else {
    _checkBlockContents(env, &fn->body, returnType);
  }

  daaCheckFn(fn);

  if (!_checkIfBodyProducesDeclaredValue(fn, returnType)) {
    typchkErrorAtTokenFmt(&fn->name, "'%.*s' must return %s on every path.",
                          fn->name.length, fn->name.start,
                          typeToString(returnType));
  }

  typeEnvSetCurrentReturnType(env, previousReturnType);
  typeEnvSetSelfType(env, previousSelfType);
  typeEnvEndScope(env);
}

static void _checkFunctionDecl(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  bool hasArity = fn->arity > 0;

  Type **paramTypes =
      hasArity ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;

  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = (fn->paramTypes != NULL && fn->paramTypes[i] != NULL)
                        ? typchkResolveType(env, fn->paramTypes[i])
                        : NULL;
  }

  Type *returnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;

  Type *fnType = typeFunction(paramTypes, fn->arity, returnType);
  typeEnvDeclare(env, fn->name, fnType);

  Type *selfType = typeEnvGetSelfType(env);

  _checkFunctionBody(env, fn, paramTypes, returnType, selfType);
}

void checkStmt(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    infer(env, node->as.exprStmt.expr);
    break;
  case NODE_PRINT:
    infer(env, node->as.print.expr);
    break;
  case NODE_VAR_DECL:
    _checkVarDecl(env, node);
    break;
  case NODE_WHILE: {
    WhileNode *w = &node->as.while_;
    check(env, w->condition, typeBool());
    checkStmt(env, w->body);
    break;
  }
  case NODE_FOR: {
    ForNode *f = &node->as.for_;
    typeEnvBeginScope(env);

    if (f->init != NULL)
      checkStmt(env, f->init);

    if (f->condition != NULL)
      check(env, f->condition, typeBool());

    checkStmt(env, f->body);

    if (f->increment != NULL)
      infer(env, f->increment);

    typeEnvEndScope(env);
    break;
  }
  case NODE_IF: {
    IfNode *if_ = &node->as.if_;

    check(env, if_->condition, typeBool());
    checkStmt(env, if_->thenBranch);

    if (if_->elseBranch != NULL)
      checkStmt(env, if_->elseBranch);

    break;
  }
  case NODE_RETURN: {
    ReturnNode *r = &node->as.return_;
    Type *expectedReturn = typeEnvGetCurrentReturnType(env);

    if (r->value != NULL) {
      if (expectedReturn != NULL)
        check(env, r->value, expectedReturn);
      else
        infer(env, r->value);
    } else if (expectedReturn != NULL &&
               !typesEqual(expectedReturn, typeUnit())) {
      _errorAtNodeFmt(node, "Expected a return value of type %s.",
                      typeToString(expectedReturn));
    }

    break;
  }
  case NODE_BREAK:
  case NODE_CONTINUE:
    break;
  case NODE_FUNCTION:
    _checkFunctionDecl(env, node);
    break;
  case NODE_STRUCT:
  case NODE_IMPL:
  case NODE_TRAIT:
  case NODE_TYPE_ALIAS:
    break;
  default:
    infer(env, node);
    break;
  }
}

// Resolves a function/method signature, requiring every param + the
// return type to have an annotation. self is excluded -- its type is
// always just "this struct," bound separately via selfType.
static Type *_resolveFunctionSignature(TypeEnv *env, FunctionNode *fn) {
  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  bool ok = true;
  for (int i = 0; i < fn->arity; i++) {
    if (fn->paramTypes == NULL || fn->paramTypes[i] == NULL) {
      typchkErrorAtTokenFmt(&fn->params[i], "Parameter '%.*s' needs a type.",
                            fn->params[i].length, fn->params[i].start);
      ok = false;
      continue;
    }
    Type *paramType = typchkResolveType(env, fn->paramTypes[i]);
    if (paramType == NULL)
      ok = false;
    paramTypes[i] = paramType;
  }

  if (fn->returnType == NULL) {
    typchkErrorAtTokenFmt(&fn->name, "'%.*s' needs a return type.",
                          fn->name.length, fn->name.start);
    ok = false;
  }
  Type *returnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;

  if (!ok)
    return NULL;
  return typeFunction(paramTypes, fn->arity, returnType);
}

// A type alias declaration waiting to be resolved. Aliases may reference
// each other in any order, so they're collected first and resolved
// on demand, depth first.
typedef struct {
  AstNode *node;
  bool resolving;
  bool resolved;
} UnresolvedTypeAlias;

static void _resolvePendingAlias(TypeEnv *env,
                                 UnresolvedTypeAlias *unresolvedAlias,
                                 int count, int index);

// Resolves any alias `typeAnnotation` names before it is itself resolved,
// so an alias declared later in the file still works.
static void _resolveAliasDependencies(TypeEnv *env,
                                      UnresolvedTypeAlias *unresolvedAlias,
                                      int count, AstNode *typeAnnotation) {
  if (typeAnnotation == NULL)
    return;

  if (typeAnnotation->kind == NODE_TYPE_FUNCTION) {
    TypeFunctionNode *fn = &typeAnnotation->as.typeFunction;
    for (int i = 0; i < fn->paramCount; i++) {
      _resolveAliasDependencies(env, unresolvedAlias, count, fn->paramTypes[i]);
    }
    _resolveAliasDependencies(env, unresolvedAlias, count, fn->returnType);
    return;
  }

  TypeNode *t = &typeAnnotation->as.type_;

  for (int i = 0; i < t->genericArgCount; i++) {
    _resolveAliasDependencies(env, unresolvedAlias, count, t->genericArgs[i]);
  }

  for (int i = 0; i < count; i++) {
    if (_tokensEqual(&unresolvedAlias[i].node->as.typeAlias.name, &t->name)) {
      _resolvePendingAlias(env, unresolvedAlias, count, i);
      return;
    }
  }
}

static void _resolvePendingAlias(TypeEnv *env,
                                 UnresolvedTypeAlias *unresolvedAlias,
                                 int count, int index) {
  UnresolvedTypeAlias *alias = &unresolvedAlias[index];
  TypeAliasNode *decl = &alias->node->as.typeAlias;

  if (alias->resolved)
    return;

  if (alias->resolving) {
    typchkErrorAtTokenFmt(&decl->name, "Type alias '%.*s' is circular.",
                          decl->name.length, decl->name.start);
    alias->resolved = true;
    return;
  }

  alias->resolving = true;
  _resolveAliasDependencies(env, unresolvedAlias, count, decl->target);
  alias->resolving = false;
  alias->resolved = true;

  Type *target = typchkResolveType(env, decl->target);

  if (target != NULL)
    typeEnvRegisterAlias(env, decl->name, target);
}

static void _resolveStructFields(TypeEnv *env, AstNode *node) {
  StructNode *struct_ = &node->as.struct_;
  Type *structType = typeEnvLookupStruct(env, struct_->name);

  if (structType == NULL)
    return;

  if (typeStructIsGeneric(structType))
    return; // already reported once at declaration; don't cascade

  UninternedTypeMember *fields =
      struct_->fieldCount > 0
          ? (UninternedTypeMember *)typesAllocRaw(struct_->fieldCount *
                                                  sizeof(UninternedTypeMember))
          : NULL;

  bool ok = true;
  for (int i = 0; i < struct_->fieldCount; i++) {
    VarDeclNode *field = &struct_->fields[i];

    if (field->declaredType == NULL) {
      typchkErrorAtTokenFmt(&field->name, "Field '%.*s' needs a type.",
                            field->name.length, field->name.start);
      ok = false;
      continue;
    }

    Type *fieldType = typchkResolveType(env, field->declaredType);

    if (fieldType == NULL) {
      ok = false;
      continue;
    }

    fields[i].name = field->name;
    fields[i].type = fieldType;
  }

  if (ok) {
    typeStructSetFields(structType, fields, struct_->fieldCount);
  } else {
    typeStructMarkUnresolvedMembers(structType);
  }
}

static void _resolveTraitMethods(TypeEnv *env, AstNode *node) {
  TraitNode *trait_ = &node->as.trait_;
  Type *traitType = typeEnvLookupTrait(env, trait_->name);

  if (traitType == NULL)
    return;

  bool ok = true;

  if (trait_->hasSupertrait) {
    Type *supertraitType = typeEnvLookupTrait(env, trait_->supertrait);

    if (supertraitType == NULL) {
      typchkErrorAtTokenFmt(&trait_->supertrait, "Unknown trait '%.*s'.",
                            trait_->supertrait.length,
                            trait_->supertrait.start);
      ok = false;
    } else {
      typeTraitSetSupertrait(traitType, supertraitType->as.trait_.name);
    }
  }

  int staticCount = 0, instanceCount = 0;
  for (int i = 0; i < trait_->methodCount; i++) {
    if (trait_->methods[i]->hasSelf)
      instanceCount++;
    else
      staticCount++;
  }

  UninternedTypeMember *staticMethods =
      staticCount > 0 ? (UninternedTypeMember *)typesAllocRaw(
                            staticCount * sizeof(UninternedTypeMember))
                      : NULL;
  UninternedTypeMember *instanceMethods =
      instanceCount > 0 ? (UninternedTypeMember *)typesAllocRaw(
                              instanceCount * sizeof(UninternedTypeMember))
                        : NULL;

  int staticIndex = 0, instanceIndex = 0;
  for (int i = 0; i < trait_->methodCount; i++) {
    FunctionNode *method = trait_->methods[i];
    Type *methodType = _resolveFunctionSignature(env, method);

    if (methodType == NULL) {
      ok = false;
      continue;
    }

    if (method->hasSelf) {
      instanceMethods[instanceIndex].name = method->name;
      instanceMethods[instanceIndex].type = methodType;
      instanceIndex++;
    } else {
      staticMethods[staticIndex].name = method->name;
      staticMethods[staticIndex].type = methodType;
      staticIndex++;
    }
  }

  if (ok) {
    typeTraitSetMethods(traitType, staticMethods, staticIndex, instanceMethods,
                        instanceIndex);
  } else {
    typeTraitMarkUnresolvedMembers(traitType);
  }
}

static Token tokenFromInternedName(InternedName name) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = internedNameChars(name);
  token.length = name.length;
  token.line = 0;
  return token;
}

// Walks the supertrait chain to identify circular references
static bool _traitSupertraitChainCycles(TypeEnv *env, InternedName startName) {
  InternedName current = startName;
  int maxSteps = typeEnvTraitCount(env) + 1;

  for (int step = 0; step < maxSteps; step++) {
    Type *currentTrait =
        typeEnvLookupTrait(env, tokenFromInternedName(current));

    if (currentTrait == NULL || !currentTrait->as.trait_.hasSupertrait)
      return false; // chain ends cleanly, no repeat

    InternedName next = currentTrait->as.trait_.supertraitName;

    if (internedNamesEqual(next, startName))
      return true; // back to where the walk started

    current = next;
  }

  return true; // walked further than there are traits -- must have repeated
}

// Checked once per trait after every trait's own supertrait field has
// been resolved, so a cycle of any length is caught regardless of which
// trait in it happens to be declared first (or checked first).
static void _checkTraitSupertraitCycle(TypeEnv *env, AstNode *node) {
  TraitNode *trait_ = &node->as.trait_;
  if (!trait_->hasSupertrait)
    return;

  Type *traitType = typeEnvLookupTrait(env, trait_->name);

  // NULL or no supertrait recorded means it already failed to resolve
  // (e.g. "Unknown trait") and was reported there -- nothing to walk.
  if (traitType == NULL || !traitType->as.trait_.hasSupertrait)
    return;

  if (_traitSupertraitChainCycles(env, traitType->as.trait_.name)) {
    typchkErrorAtTokenFmt(&trait_->name,
                          "Trait '%.*s' has a circular supertrait chain.",
                          trait_->name.length, trait_->name.start);
  }
}

// Resolves an impl block's target name to the concrete struct directly, or
// through a type alias. NULL if the name doesn't name either
static Type *_resolveImplTarget(TypeEnv *env, Token name) {
  Type *type = typeEnvLookupStruct(env, name);
  if (type != NULL)
    return type;

  type = typeEnvLookupAlias(env, name);

  return (type != NULL && type->kind == TYPE_STRUCT) ? type : NULL;
}

static void _registerTraitImpl(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  Type *traitType = typeEnvLookupTrait(env, impl->traitName);

  if (traitType == NULL) {
    typchkErrorAtTokenFmt(&impl->traitName, "Unknown trait '%.*s'.",
                          impl->traitName.length, impl->traitName.start);
    return;
  }

  if (typeTraitHasUnresolvedMembers(traitType))
    return; // already reported once, at the trait's own declaration

  Type *targetType = _resolveImplTarget(env, impl->targetName);

  if (targetType == NULL) {
    if (_tokenIsPrimitiveTypeName(&impl->targetName)) {
      typchkErrorAtTokenFmt(
          &impl->targetName,
          "Primitive trait implementations aren't supported yet.");
    } else {
      typchkErrorAtTokenFmt(&impl->targetName, "Unknown type '%.*s'.",
                            impl->targetName.length, impl->targetName.start);
    }

    return;
  }

  resolvedImplTargetsRecord(node, targetType->as.struct_.name);

  if (typeStructIsGeneric(targetType))
    return; // already reported once at the struct's declaration

  InternedName traitName = internTokenName(impl->traitName);

  if (typeStructImplementsTrait(targetType, traitName)) {
    typchkErrorAtTokenFmt(&impl->traitName, "'%.*s' already implements '%.*s'.",
                          impl->targetName.length, impl->targetName.start,
                          impl->traitName.length, impl->traitName.start);
    return;
  }

  bool ok = true;

  // Register the impl's target type e.g. struct for `Self`
  typeEnvSetImplTargetType(env, targetType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];

    Type *requiredType =
        method->hasSelf ? typeTraitInstanceMethodLookup(traitType, method->name)
                        : typeTraitStaticMethodLookup(traitType, method->name);

    if (requiredType == NULL) {
      Type *otherCategory =
          method->hasSelf
              ? typeTraitStaticMethodLookup(traitType, method->name)
              : typeTraitInstanceMethodLookup(traitType, method->name);

      if (otherCategory != NULL) {
        typchkErrorAtTokenFmt(
            &method->name,
            method->hasSelf
                ? "'%.*s' is a static method on trait '%.*s' -- drop 'self'."
                : "'%.*s' is an instance method on trait '%.*s' -- add "
                  "'self'.",
            method->name.length, method->name.start, impl->traitName.length,
            impl->traitName.start);
      } else {
        typchkErrorAtTokenFmt(&method->name,
                              "'%.*s' isn't a method defined by trait '%.*s'.",
                              method->name.length, method->name.start,
                              impl->traitName.length, impl->traitName.start);
      }
      ok = false;
      continue;
    }

    Type *methodType = _resolveFunctionSignature(env, method);
    if (methodType == NULL) {
      ok = false;
      continue;
    }

    Type *concreteMethodType = typeSubstituteSelf(methodType, targetType);
    Type *concreteRequiredType = typeSubstituteSelf(requiredType, targetType);

    if (!typesEqual(concreteMethodType, concreteRequiredType)) {
      typchkErrorAtTokenFmt(
          &method->name,
          "'%.*s' doesn't match trait '%.*s': expected %s, got %s.",
          method->name.length, method->name.start, impl->traitName.length,
          impl->traitName.start, typeToString(concreteRequiredType),
          typeToString(concreteMethodType));
      ok = false;
      continue;
    }

    typeStructAddTraitMethod(targetType, method->name, concreteMethodType,
                             method->hasSelf);
  }

  typeEnvSetImplTargetType(env, NULL);

  for (int i = 0; i < typeTraitInstanceMethodCount(traitType); i++) {
    TypeMember required = typeTraitInstanceMethodAt(traitType, i);
    bool found = false;
    for (int j = 0; j < impl->methodCount && !found; j++) {
      found = impl->methods[j]->hasSelf &&
              internedNameEqualsToken(required.name, impl->methods[j]->name);
    }
    if (!found) {
      typchkErrorAtTokenFmt(&impl->traitName,
                            "'%.*s' is missing '%.*s', required by trait "
                            "'%.*s'.",
                            impl->targetName.length, impl->targetName.start,
                            required.name.length,
                            internedNameChars(required.name),
                            impl->traitName.length, impl->traitName.start);
      ok = false;
    }
  }
  for (int i = 0; i < typeTraitStaticMethodCount(traitType); i++) {
    TypeMember required = typeTraitStaticMethodAt(traitType, i);
    bool found = false;
    for (int j = 0; j < impl->methodCount && !found; j++) {
      found = !impl->methods[j]->hasSelf &&
              internedNameEqualsToken(required.name, impl->methods[j]->name);
    }
    if (!found) {
      typchkErrorAtTokenFmt(&impl->traitName,
                            "'%.*s' is missing '%.*s', required by trait "
                            "'%.*s'.",
                            impl->targetName.length, impl->targetName.start,
                            required.name.length,
                            internedNameChars(required.name),
                            impl->traitName.length, impl->traitName.start);
      ok = false;
    }
  }

  if (!ok)
    return;

  typeStructMarkTraitImplemented(targetType, traitName);
}

static void _checkTraitSupertraitSatisfied(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  if (!impl->hasTraitName)
    return;

  Type *traitType = typeEnvLookupTrait(env, impl->traitName);
  if (traitType == NULL || !traitType->as.trait_.hasSupertrait)
    return;

  Type *targetType = _resolveImplTarget(env, impl->targetName);
  if (targetType == NULL)
    return; // already reported, or a (currently unsupported) primitive

  InternedName traitName = internTokenName(impl->traitName);

  if (!typeStructImplementsTrait(targetType, traitName))
    return;

  if (!typeStructImplementsTrait(targetType,
                                 traitType->as.trait_.supertraitName)) {
    typchkErrorAtTokenFmt(
        &impl->traitName,
        "'%.*s' also needs 'impl %.*s for %.*s' -- '%.*s' requires it.",
        impl->targetName.length, impl->targetName.start,
        traitType->as.trait_.supertraitName.length,
        internedNameChars(traitType->as.trait_.supertraitName),
        impl->targetName.length, impl->targetName.start, impl->traitName.length,
        impl->traitName.start);
  }
}

static void _registerImplMethods(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  if (impl->hasTraitName) {
    _registerTraitImpl(env, node);
    return;
  }

  Type *structType = _resolveImplTarget(env, impl->targetName);

  if (structType == NULL) {
    if (_tokenIsPrimitiveTypeName(&impl->targetName)) {
      typchkErrorAtTokenFmt(
          &impl->targetName,
          "Only trait implementations are allowed on primitive types.");
    } else {
      typchkErrorAtTokenFmt(&impl->targetName, "Unknown type '%.*s'.",
                            impl->targetName.length, impl->targetName.start);
    }
    return;
  }

  resolvedImplTargetsRecord(node, structType->as.struct_.name);

  if (typeStructIsGeneric(structType))
    return; // already reported once at the struct's declaration

  typeEnvSetImplTargetType(env, structType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = _resolveFunctionSignature(env, method);

    if (methodType == NULL) {
      typeStructMarkUnresolvedMembers(structType); // error already reported
      continue;
    }

    Type *existing =
        method->hasSelf
            ? typeStructInstanceMethodLookup(structType, method->name)
            : typeStructStaticMethodLookup(structType, method->name);
    if (existing != NULL) {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' is already declared on '%.*s'.",
                            method->name.length, method->name.start,
                            impl->targetName.length, impl->targetName.start);
      continue;
    }

    if (method->hasSelf) {
      typeStructAddInstanceMethod(structType, method->name, methodType,
                                  method->isPublic);
    } else {
      typeStructAddStaticMethod(structType, method->name, methodType,
                                method->isPublic);
    }
  }

  typeEnvSetImplTargetType(env, NULL);
}

static void _registerTopLevelFunctionSignature(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = _resolveFunctionSignature(env, fn);

  if (fnType != NULL) {
    typeEnvRegisterFunction(env, fn->name, fnType);
  }
}

static void _checkTopLevelFunctionBody(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = typeEnvLookupFunction(env, fn->name);
  if (fnType == NULL)
    return; // signature failed to resolve in Pass D; already reported
  _checkFunctionBody(env, fn, fnType->as.function.paramTypes,
                     fnType->as.function.returnType, NULL);
}

static void _checkImplMethodBodies(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  Type *structType = _resolveImplTarget(env, impl->targetName);

  typeEnvSetImplTargetType(env, structType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = NULL;

    if (structType != NULL) {
      if (impl->hasTraitName) {
        methodType =
            method->hasSelf
                ? typeStructTraitInstanceMethodLookup(structType, method->name)
                : typeStructTraitStaticMethodLookup(structType, method->name);
      } else if (method->hasSelf) {
        methodType = typeStructInstanceMethodLookup(structType, method->name);
      } else {
        methodType = typeStructStaticMethodLookup(structType, method->name);
      }
    }

    if (methodType == NULL)
      continue; // signature/struct/trait validation failed; already reported

    Type *selfType = method->hasSelf ? structType : NULL;
    _checkFunctionBody(env, method, methodType->as.function.paramTypes,
                       methodType->as.function.returnType, selfType);
  }

  typeEnvSetImplTargetType(env, NULL);
}

bool typchkCheckProgram(AstNode **program, int count) {
  // Diagnostics are per-unit/program
  _resetHadTypecheckError();

  bool ownsEnv = sessionEnv == NULL;
  TypeEnv *env = ownsEnv ? typeEnvInit() : sessionEnv;

  if (ownsEnv)
    typeEnvBeginScope(env);

  // Structs

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      StructNode *sn = &program[i]->as.struct_;
      Type *placeholder = typeStruct(sn->name, NULL, 0, NULL, 0, NULL, 0);

      if (sn->genericParamCount > 0) {
        typeStructMarkGeneric(placeholder);
        typchkErrorAtToken(&sn->name, "Generic structs aren't supported yet.");
      }

      typeEnvRegisterStruct(env, sn->name, placeholder);
    }
  }

  bool *isDuplicateTrait =
      count > 0 ? (bool *)calloc((size_t)count, sizeof(bool)) : NULL;

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      TraitNode *trait_ = &program[i]->as.trait_;

      Type *existing = typeEnvLookupTrait(env, trait_->name);

      if (existing != NULL) {
        if (typeTraitIsBuiltin(existing)) {
          typchkErrorAtTokenFmt(
              &trait_->name,
              "'%.*s' is a builtin trait and can't be redeclared.",
              trait_->name.length, trait_->name.start);
        } else {
          typchkErrorAtTokenFmt(&trait_->name,
                                "'%.*s' is already declared as a trait.",
                                trait_->name.length, trait_->name.start);
        }

        if (isDuplicateTrait != NULL) {
          isDuplicateTrait[i] = true;
        }

        continue;
      }

      Type *placeholder = typeTrait(trait_->name, NULL, 0, NULL, 0);
      typeEnvRegisterTrait(env, trait_->name, placeholder);
    }
  }

  // Type aliases
  //
  // After struct placeholders so an alias can name a struct, before struct
  // fields so a field can be annotated with an alias.

  // TODO: Refactor this

  UnresolvedTypeAlias *unresolvedAliasAliases = NULL;
  int pendingAliasCount = 0;

  for (int i = 0; i < count; i++) {
    // Generic aliases are still parse-only, same as generic types.
    if (program[i]->kind == NODE_TYPE_ALIAS &&
        program[i]->as.typeAlias.genericParamCount == 0) {
      pendingAliasCount++;
    }
  }

  if (pendingAliasCount > 0) {
    unresolvedAliasAliases = (UnresolvedTypeAlias *)calloc(
        (size_t)pendingAliasCount, sizeof(UnresolvedTypeAlias));

    int next = 0;
    for (int i = 0; i < count; i++) {
      if (program[i]->kind == NODE_TYPE_ALIAS &&
          program[i]->as.typeAlias.genericParamCount == 0) {
        unresolvedAliasAliases[next++].node = program[i];
      }
    }

    for (int i = 0; i < pendingAliasCount; i++) {
      _resolvePendingAlias(env, unresolvedAliasAliases, pendingAliasCount, i);
    }

    free(unresolvedAliasAliases);
  }

  // Trait method signatures + supertrait resolution
  //
  // After aliases, so a trait method can be annotated with one; after
  // struct placeholders, so a trait method can reference a struct by name.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT &&
        !(isDuplicateTrait != NULL && isDuplicateTrait[i])) {
      _resolveTraitMethods(env, program[i]);
    }
  }

  free(isDuplicateTrait);

  // Supertrait cycle detection
  //
  // After every trait's own supertrait field is resolved, so a chain
  // (however many traits long) can be walked in either direction
  // regardless of declaration order.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      _checkTraitSupertraitCycle(env, program[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      _resolveStructFields(env, program[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      _registerImplMethods(env, program[i]);
    }
  }

  // Supertrait satisfaction
  //
  // After every impl in the program has been registered, so `impl Ord for
  // X` is checked against `impl Eq for X` regardless of which appears
  // first in the file.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      _checkTraitSupertraitSatisfied(env, program[i]);
    }
  }

  // Hoisted Functions

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_FUNCTION) {
      _registerTopLevelFunctionSignature(env, program[i]);
    }
  }

  // Definite Assignment Analysis

  DaaSet topLevelDaa;
  daaSetInit(&topLevelDaa);

  for (int i = 0; i < count; i++) {
    AstNode *node = program[i];

    switch (node->kind) {
    case NODE_FUNCTION:
      _checkTopLevelFunctionBody(env, node);
      break;

    case NODE_IMPL:
      _checkImplMethodBodies(env, node);
      break;

    default:
      checkStmt(env, node);
      break;
    }

    daaCheckAssignmentStmt(&topLevelDaa, node);
  }

  daaSetFree(&topLevelDaa);

  // Clean Up

  bool ok = !_hadTypecheckError();

  if (ownsEnv) {
    typeEnvEndScope(env);
    typeEnvFree(env);
  }

  return ok;
}
