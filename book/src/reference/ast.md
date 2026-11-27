# AST

## Generate an AST

```c
// src/parser.h

/**
 * Parses `source` into a flat array of AST nodes.
 *
 * The caller must call `astFreeAll()` to free AST arena memory after use.
 *
 * @param source The source code to parse
 * @param outCount The number of top-level declarations parsed.
 * @param hadError If any parse error was reported.
 * @param outEndLine The line of the EOF token
 */
AstNode **parse(const char *source, int *outCount, bool *hadError,
                int *outEndLine);

```

```c
// src/ast.h

/**
 * The `parse` caller calls this to free the AST.
 */
void astFreeAll(void);
```

### AstNode

```c
struct AstNode {
  NodeKind kind;
  int line;
  union {
    LiteralNode literal;
    UnaryNode unary;
    BinaryNode binary;
    GroupingNode grouping;
    VariableNode variable;
    AssignNode assign;
    LogicalNode logical;
    CallNode call;
    GetNode get;
    SetNode set;
    IndexGetNode indexGet;
    IndexSetNode indexSet;
    SelfNode self_;
    ExprStmtNode exprStmt;
    VarDeclNode varDecl;
    BlockNode block;
    IfNode if_;
    WhileNode while_;
    ForNode for_;
    ReturnNode return_;
    FunctionNode function;
    StructNode struct_;
    StructInitNode structInit;
    ImplNode impl;
    TraitNode trait_;
    ArrayNode array;
    BreakNode break_;
    ContinueNode continue_;
    TypeNode type_;
    TypeAliasNode typeAlias;
    TypeFunctionNode typeFunction;
    TypeListNode typeList;
    TypeArrayNode typeArray;
    InterpStringNode interpString;
    CastNode cast;
  } as;
};
```

### Literals

```c
typedef enum {
  LITERAL_NIL,
  LITERAL_UNIT,
  LITERAL_BOOL,
  LITERAL_NUMBER,
  LITERAL_STRING
} LiteralKind;

typedef struct {
  LiteralKind kind;
  union {
    bool boolean;
    struct {
      NumberKind kind;
      NumberPayload as;
      /**
       * A plain, unsuffixed integer literal, e.g. `42` -- as opposed to a
       * suffixed (`42f64`, `42u8`) or decimal (`1.5`) literal. Its kind is
       * i64 unless the type checker changes it to the kind that is expected.
       */
      bool literalIsPlainInteger;
      /**
       * For a plain integer literal, its exact value, unless it does not fit
       * in 64 bits (`literalValueFits` is false).
       */
      uint64_t literalValue;
      bool literalValueFits;
    } number;
    struct {
      const char *chars;
      int length;
    } string;
  } as;
} LiteralNode;
```

### Operators

```c
typedef struct {
  Token op;
  AstNode *operand;
  /**
   * Set by the type checker when the operand is a struct or a generic
   * parameter, so `-a` calls the operand's trait method. NULL for a number.
   */
  const OperatorTrait *traitDispatch;
} UnaryNode;

typedef struct {
  Token op;
  AstNode *left;
  AstNode *right;
  /**
   * `+` on two strings. The compiler has no types, so the type checker sets
   * this to let it join chains of strings with @strConcat.
   */
  bool isStringConcat;
  /**
   * Set by the type checker when the operands are structs or generic
   * parameters, so the operator calls the left operand's trait method. NULL
   * for numbers and strings.
   */
  const OperatorTrait *traitDispatch;
} BinaryNode;

/** One row of the operator table in `operator_traits.h`. */
typedef struct {
  TokenType token;
  const char *trait;
  const char *method;
  bool isUnary;
} OperatorTrait;
```

### Grouping Expression

```c
typedef struct {
  AstNode *inner;
} GroupingNode;
```

### Identifiers/Bindings

```c
typedef struct {
  Token name;
  /**
   * Set by the type checker when the name is a type alias of a struct, as in
   * `Alias.method()`: the struct to read instead. Its length is 0 otherwise.
   */
  Token structName;
} VariableNode;
```

### Assignment

```c
typedef struct {
  Token name;
  AstNode *value;
} AssignNode;
```

### And/Or

```c
typedef struct {
  AstNode *left;
  AstNode *right;
} LogicalNode;
```

### Expression Statements

Expressions followed by a semicolon.

```c
typedef struct {
  AstNode *expr;
} ExprStmtNode;
```

### Calling Functions

```c
typedef struct {
  AstNode *callee;
  /**
   * closing ')' — used for error location
   */
  Token paren;
  AstNode **args;
  int argCount;
  /**
   * A call to @print, @println, @eprint, or @eprintln whose argument's type
   * implements Display, so it's converted with toString() first. Natives
   * can't call Kirby methods, so the compiler does it. Set by the type
   * checker, because the compiler has no types.
   */
  bool argUsesDisplay;
  /**
   * `Trait.method(value, ...)`: the callee's object names the trait, and the
   * first argument is the value the method runs on. Set by the type checker.
   */
  bool isTraitCall;
} CallNode;
```

### Getting/Setting Instance Fields

```c
typedef struct {
  AstNode *object;
  Token name;
  /**
   * Set by the type checker when the call is to a method of a primitive type,
   * such as "string". NULL for every other get. The compiler turns the call
   * into a plain function call, because it has no types of its own.
   */
  const char *primitiveOwner;
  // For such a call, the name of the trait the method came from. NULL when it
  // is a method of the type itself.
  const char *primitiveTrait;
  // For such a call, true when the method is static, so the call has no
  // receiver value, as in `f64.default()`
  bool primitiveIsStatic;
  /**
   * Set by the type checker on a call of a trait's method that must run that
   * trait's version, even when the receiver has a method of its own with the
   * same name: a call through a type parameter's bound, and `Trait.method(x)`.
   * The name of the trait. NULL for every other get.
   */
  const char *viaTrait;
} GetNode;
```

```c
typedef struct {
  AstNode *object;
  Token name;
  AstNode *value;
} SetNode;
```

### Index Accessing

```c
typedef struct {
  AstNode *object;
  AstNode *index;
  /**
   * opening '[' — used for error location
   */
  Token bracket;
} IndexGetNode;

typedef struct {
  AstNode *object;
  AstNode *index;
  AstNode *value;
  Token bracket;
} IndexSetNode;
```

### Types

```c
typedef struct {
  Token name;
  /**
   * NODE_TYPE nodes
   */
  AstNode **genericArgs;
  int genericArgCount;
} TypeNode;

typedef struct {
  Token name;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `type Wrapper[T] = T;`. Distinct from TypeNode.genericArgs --
   * these are parameter *declarations* (bare names), not type arguments.
   */
  Token *genericParams;
  int genericParamCount;
  /** The traits each parameter must implement. NULL when there are no
   * parameters. */
  GenericBounds *genericBounds;
  /**
   * The type expression on the right of `=`.
   */
  AstNode *target;
} TypeAliasNode;
```

```c
/**
 * A function-type expression, e.g. `fun (i64, i64) => i64`. Distinct from
 * TypeNode -- a function type has no name to hang nominal identity on, it's
 * compared structurally (params + return) instead.
 */
typedef struct {
  /**
   * arena-allocated array of NODE_TYPE(-like) nodes
   */
  AstNode **paramTypes;
  int paramCount;
  /**
   * Never NULL -- the return type is mandatory on a function type, same as
   * on a real function declaration.
   */
  AstNode *returnType;
} TypeFunctionNode;

/**
 * A list type, e.g. `[i64]`.
 */
typedef struct {
  AstNode *elementType;
} TypeListNode;

/**
 * A fixed size array type, e.g. `[i64; 3]`.
 */
typedef struct {
  AstNode *elementType;
  int length;
  // The standard library can name the length instead, as in `[T; N]`
  bool hasLengthName;
  Token lengthName;
} TypeArrayNode;
```

### Let/Var Bindings

```c
typedef struct {
  Token name;
  AstNode *initializer;
  /**
   * Type annotation, if present.
   *
   * NULL if not present.
   */
  AstNode *declaredType;
  int declEndLine;
  bool isMutable;
  bool isPublic;
} VarDeclNode;
```

### Block Statement

```c
typedef struct {
  AstNode **stmts;
  int count;
  AstNode *value;
  int endLine;
} BlockNode;
```

### Flow Control

```c
typedef struct {
  AstNode *condition;
  AstNode *thenBranch;
  /**
   * NULL if no else clause
   */
  AstNode *elseBranch;
} IfNode;
```

```c
typedef struct {
  AstNode *init;
  AstNode *condition;
  AstNode *body;
  AstNode *increment;
} ForNode;
```

```c
typedef struct {
  AstNode *condition;
  AstNode *body;
} WhileNode;
```

```c
typedef struct {
  Token token;
} BreakNode;
```

```c
typedef struct {
  Token token;
} ContinueNode;
```

```c
typedef struct {
  AstNode *value;
} ReturnNode;
```

### Generic Bounds

```c
/**
 * The traits one generic parameter must implement, as written after the `:`
 * in `T: Display + Eq`. `count` is 0 for a parameter with no bound.
 */
typedef struct {
  Token *traits;
  int count;
} GenericBounds;
```

`krb parse` prints a bound after its parameter, as `T:Display+Eq`. Functions,
structs and type aliases have bounds. An `impl` doesn't: a struct's impl takes
them from its struct, and a list or array impl has none.

### Functions

```c
typedef struct {
  Token name;
  /**
   * arena-allocated array of Token
   */
  Token *params;
  /**
   * Function parameter types, if present.
   *
   * NULL if not present on param.
   */
  AstNode **paramTypes;
  int arity;
  BlockNode body;    // used when exprBody == NULL;
  AstNode *exprBody; // non-NULL for `fun name(...) = expr;` bodies
  /**
   * Function return type, if present.
   *
   * NULL if not present.
   */
  AstNode *returnType;
  int bodyEndLine;
  bool isMethod;
  bool hasSelf;
  bool isLambda;
  bool isPublic;
  // Is this a fun declaration in a trait? e.g. no body
  bool isTraitSignature;
  bool isNative; // A standard library declaration of a function written in C
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `fun sum[T](a: T, b: T): T`. NULL/0 for a non-generic function.
   * Never set for lambdas -- there's no name slot to attach `[T]` to.
   */
  Token *genericParams;
  int genericParamCount;
  /**
   * The traits each generic parameter must implement, such as the `Display` in
   * `fun show[T: Display](x: T)`. Has one entry per generic parameter, with a
   * count of 0 for a parameter with no bound. NULL when `genericParamCount` is
   * 0.
   */
  GenericBounds *genericBounds;
  /** Which parameters are lengths, declared as `const N`. NULL when none can
   * be. */
  bool *genericIsConst;
} FunctionNode;
```

### Structs

```c
typedef struct {
  Token name;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `struct Box[T]`. NULL/0 for a non-generic struct.
   */
  Token *genericParams;
  int genericParamCount;
  /** The traits each parameter must implement. NULL when there are no
   * parameters. */
  GenericBounds *genericBounds;
  VarDeclNode *fields;
  int fieldCount;
  int endLine;
} StructNode;
```

```c
typedef struct {
  Token name;
  AstNode *value;
} StructInitFieldNode;
```

```c
typedef struct {
  Token name;
  StructInitFieldNode *fields;
  int fieldCount;
  int endLine;
  // The struct's name when `name` is a type alias of it, as for VariableNode
  Token structName;
} StructInitNode;
```

### Impls And Traits

```c
// What an impl block is for
typedef enum {
  IMPL_TARGET_NAME,  // `impl Box[T]`, `impl string`
  IMPL_TARGET_LIST,  // `impl[T] [T]`
  IMPL_TARGET_ARRAY, // `impl[T, const N] [T; N]`
} ImplTargetKind;

typedef struct {
  ImplTargetKind targetKind;
  /**
   * The type's name, or the `[` token for a list or array target.
   */
  Token targetName;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `impl Box[T]` or `impl[T] [T]`. A struct impl has as many as the
   * struct declares. They stand for the struct's parameters in order, and the
   * names can differ. A list or array impl declares its own after `impl`.
   */
  Token *genericParams;
  int genericParamCount;
  /** Which parameters are lengths, declared as `const N`. NULL for a struct
   * impl. */
  bool *genericIsConst;
  /** The names in a list or array target: the item type's, and for an array
   * the length's, which can be `_`. */
  Token itemName;
  Token lengthName;
  // impl Trait for Struct?
  bool hasTraitName;
  Token traitName;
  FunctionNode **methods;
  int methodCount;
  int endLine;
  /**
   * Set by the type checker when the block adds methods to a primitive type,
   * such as "string" or "u8", or implements a trait for one. NULL for every
   * other impl.
   */
  const char *primitiveOwner;
} ImplNode;
```

```c
typedef struct {
  Token name;
  bool hasSupertrait;
  Token supertrait;
  /**
   * arena-allocated array of FunctionNode pointers, each with
   * isTraitSignature == true -- required methods with no body.
   */
  FunctionNode **methods;
  int methodCount;
  int endLine;
  bool isBuiltin;
} TraitNode;
```

```c
typedef struct {
  Token keyword;
} SelfNode;
```

### Arrays

```c
/**
 * Called in the parser's arrayLiteral function
 *
 * It is heap-allocated via realloc and must be
 * freed with arrayNodeDataFree() once its contents have been copied into
 * the AST arena — it does not live inside AstNode itself. See ArrayNode
 * below, which holds the arena-owned copy.
 */
typedef struct {
  AstNode **data;
  int count;
  int capacity;
} ArrayNodeData;
```

```c
typedef struct {
  /**
   * arena-allocated array of element node pointers
   */
  AstNode **items;
  int count;
  /**
   * Set by the type checker when the literal makes a fixed size array rather
   * than a list.
   */
  bool isFixedSize;
} ArrayNode;
```

### Casting

`expr as Type` -- an explicit conversion between number types, e.g.
`value as u8`.

```c
typedef struct {
  AstNode *expr;
  /**
   * The cast's target type, e.g. the `T` in `value as T`
   */
  AstNode *type;
  /**
   * The `as` keyword — used for error location.
   */
  Token as;
  /**
   * The target's number kind, set by the type checker because the compiler
   * has no types. Valid for casts the type checker accepted, which are
   * always to a number kind.
   */
  NumberKind target;
} CastNode;
```

### Interpolated Strings

`$"Hello {name}!"`. The type checker sets each part's `conversion`, and the
compiler joins the parts with `@strConcat`.

```c
/**
 * How a value becomes a string, e.g. in an interpolated string.
 */
typedef enum {
  STRING_CONVERSION_UNDEFINED, // not yet chosen by the type checker
  STRING_CONVERSION_STRING,    // already a string
  STRING_CONVERSION_NUMBER,    // @numberToString
  STRING_CONVERSION_BOOL,      // @boolToString
  STRING_CONVERSION_DISPLAY,   // toString(), from the type's Display impl
} StringConversion;

typedef struct {
  AstNode *expr;
  /**
   * Set by the type checker, because the compiler has no types to choose a
   * conversion from.
   */
  StringConversion conversion;
} InterpPart;

/**
 * `$"Hello {name}!"`
 */
typedef struct {
  /**
   * arena-allocated. Literal text (NODE_LITERAL strings) and placeholder
   * expressions in source order. Empty text is left out, so `$"{a}{b}"`
   * joins two parts, not three.
   */
  InterpPart *parts;
  int count;
} InterpStringNode;
```

```c
void arrayNodeDataInit(ArrayNodeData *and);
void arrayNodeDataWrite(ArrayNodeData *and, AstNode *item);
void arrayNodeDataFree(ArrayNodeData *and);
```

### NodeKind

| Idx | Token                |
| --: | :------------------- |
|   0 | `NODE_LITERAL`       |
|   1 | `NODE_UNARY`         |
|   2 | `NODE_BINARY`        |
|   3 | `NODE_GROUPING`      |
|   4 | `NODE_VARIABLE`      |
|   5 | `NODE_ASSIGN`        |
|   6 | `NODE_AND`           |
|   7 | `NODE_OR`            |
|   8 | `NODE_NULLISH`       |
|   9 | `NODE_CALL`          |
|  10 | `NODE_GET`           |
|  11 | `NODE_SET`           |
|  12 | `NODE_SELF`          |
|  13 | `NODE_INDEX_GET`     |
|  14 | `NODE_INDEX_SET`     |
|  15 | `NODE_EXPR_STMT`     |
|  16 | `NODE_VAR_DECL`      |
|  17 | `NODE_BLOCK`         |
|  18 | `NODE_IF`            |
|  19 | `NODE_WHILE`         |
|  20 | `NODE_FOR`           |
|  21 | `NODE_RETURN`        |
|  22 | `NODE_FUNCTION`      |
|  23 | `NODE_STRUCT`        |
|  24 | `NODE_STRUCT_INIT`   |
|  25 | `NODE_IMPL`          |
|  26 | `NODE_TRAIT`         |
|  27 | `NODE_ARRAY`         |
|  28 | `NODE_BREAK`         |
|  29 | `NODE_CONTINUE`      |
|  30 | `NODE_TYPE`          |
|  31 | `NODE_TYPE_ALIAS`    |
|  32 | `NODE_TYPE_FUNCTION` |
|  33 | `NODE_TYPE_LIST`     |
|  34 | `NODE_TYPE_ARRAY`    |
|  35 | `NODE_INTERP_STRING` |
|  36 | `NODE_CAST`          |
