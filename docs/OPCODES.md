# Bytecode

---

## Opcode Table

| Index | Opcode                  | Operand(s)                         | Stack | Notes                                             |
| ----: | ----------------------- | ---------------------------------- | ----: | ------------------------------------------------- |
|     0 | **OP_CONSTANT**         | `index`                            |    +1 |                                                   |
|     1 | **OP_NIL**              |                                    |    +1 |                                                   |
|     2 | **OP_TRUE**             |                                    |    +1 |                                                   |
|     3 | **OP_FALSE**            |                                    |    +1 |                                                   |
|     4 | **OP_ADD**              |                                    | -2 +1 |                                                   |
|     5 | **OP_SUBTRACT**         |                                    | -2 +1 |                                                   |
|     6 | **OP_MULTIPLY**         |                                    | -2 +1 |                                                   |
|     7 | **OP_DIVIDE**           |                                    | -2 +1 |                                                   |
|     8 | **OP_MODULO**           |                                    | -2 +1 |                                                   |
|     9 | **OP_NEGATE**           |                                    | -1 +1 |                                                   |
|    10 | **OP_PRINT**            |                                    |       |                                                   |
|    11 | **OP_RETURN**           |                                    |       |                                                   |
|    12 | **OP_EQUAL**            |                                    | -2 +1 |                                                   |
|    13 | **OP_NOT**              |                                    | -1 +1 |                                                   |
|    14 | **OP_POP**              |                                    |    -1 |                                                   |
|    15 | **OP_CLOSE_UPVALUE**    |                                    |       |                                                   |
|    16 | **OP_DEFINE_GLOBAL**    | `index`                            |       |                                                   |
|    17 | **OP_GET_GLOBAL**       | `index`                            |       |                                                   |
|    18 | **OP_SET_GLOBAL**       | `index`                            |       |                                                   |
|    19 | **OP_GET_UPVALUE**      | `slot`                             |       |                                                   |
|    20 | **OP_SET_UPVALUE**      | `slot`                             |       |                                                   |
|    21 | **OP_GET_LOCAL**        | `slot`                             |       |                                                   |
|    22 | **OP_SET_LOCAL**        | `slot`                             |       |                                                   |
|    23 | **OP_JUMP_IF_FALSE**    | `jumpOffset`                       |       |                                                   |
|    24 | **OP_JUMP_IF_NOT_NIL**  | `jumpOffset`                       |       |                                                   |
|    25 | **OP_JUMP**             | `jumpOffset`                       |       |                                                   |
|    26 | **OP_LOOP**             | `jumpOffset`                       |       |                                                   |
|    27 | **OP_GREATER**          |                                    | -2 +1 |                                                   |
|    28 | **OP_LESS**             |                                    | -2 +1 |                                                   |
|    29 | **OP_CALL**             | `argCount`                         |       |                                                   |
|    30 | **OP_CLOSURE**          | `index`, `isLocal`, `upvalueIndex` |       | See [below](#op_closure).                         |
|    31 | **OP_STRUCT**           | `index`                            |       |                                                   |
|    32 | **OP_STRUCT_INIT**      | `initFieldCount`                   |       |                                                   |
|    33 | **OP_FIELD**            | `index`, `isPublic`                |       |                                                   |
|    34 | **OP_GET_PROPERTY**     | `index`                            |       |                                                   |
|    35 | **OP_SET_PROPERTY**     | `index`                            |       |                                                   |
|    36 | **OP_METHOD**           | `index`                            |       |                                                   |
|    37 | **OP_INVOKE**           | `index`, `argCount`                |       |                                                   |
|    38 | **OP_ARRAY**            | `itemCount`                        | -n +1 |                                                   |
|    39 | **OP_GET_INDEX**        |                                    | -2 +1 |                                                   |
|    40 | **OP_SET_INDEX**        |                                    | -3 +1 |                                                   |
|    41 | **OP_CLOSE_BLOCK_EXPR** | `upvalueCount`                     | -n +1 | Cleans up upvalues. Leaves return value on stack. |

### OP_CLOSURE

Wraps the ObjFunction at constant index in a new closure, capturing N upvalues.

```
OP_CLOSURE  index  [isLocal upvalueIndex] × N
```

N is not encoded in the bytecode. It is read from the ObjFunction itself (function->upvalueCount) after loading the constant. Any code that walks the instruction stream — the VM, the disassembler, a GC that scans code — must resolve the constant before it can know where this instruction ends. Total length is 2 + 2N bytes.

Each pair describes one captured variable:

#### `isLocal` Meaning

- `1` Capture a local of the immediately enclosing function; upvalueIndex is its stack slot in that frame.
- `0` Capture an upvalue of the enclosing function; upvalueIndex indexes that function's upvalue array.

Pairs are emitted in the order the compiler resolved them, so pair i becomes upvalue i of the new closure — that's the index OP_GET_UPVALUE and OP_SET_UPVALUE later use.

Stack: +1 (the closure). The function itself comes from the constant table, not the stack.

### Operand Encodings

| Operand type       | Size       | Description                                                                                                                                                                                                                                                                                                                                                   |
| ------------------ | ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **index**          | `uint8_t`  | Index into the chunk’s constant table.                                                                                                                                                                                                                                                                                                                        |
| **slot**           | `uint8_t`  | Slot number in the current call frame (local or upvalue).                                                                                                                                                                                                                                                                                                     |
| **argCount**       | `uint8_t`  | FNumber of arguments pushed on the stack for a call. Used by `OP_CALL` (sole operand) and `OP_INVOKE` (second operand, after the method-name constant index). The arguments sit on top of the stack, above the callee/receiver.                                                                                                                               |
| **itemCount**      | `uint8_t`  | For `OP_ARRAY` only: determines how many values on the stack go in to the array                                                                                                                                                                                                                                                                               |
| **upvalueCount**   | `uint8_t`  | For `OP_CLOSE_BLOCK_EXPR` only: determines how many upvalues to discard                                                                                                                                                                                                                                                                                       |
| **isLocal**        | `uint8_t`  | (0 or 1) For `OP_CLOSURE` only: tells whether the captured variable lives in the immediate surrounding function (`1`) or is itself an upvalue (`0`).                                                                                                                                                                                                          |
| **upvalueIndex**   | `uint8_t`  | Index of the local or upvalue being captured (used together with the byte above).                                                                                                                                                                                                                                                                             |
| **jumpOffset**     | `uint16_t` | Unsigned relative jump distance, big-endian (high byte first). Measured from the start of the instruction following the operand — i.e. from `offset + 3`. `OP_JUMP`, `OP_JUMP_IF_FALSE`, and `OP_JUMP_IF_NOT_NIL` add it (forward); `OP_LOOP` subtracts it (backward). Because the value is unsigned, direction is determined by the opcode, not the operand. |
| **isPublic**       | `uint8_t`  | A flag operand. 0 = false, 1 = true                                                                                                                                                                                                                                                                                                                           |
| **initFieldCount** | `uint8_t`  | The number of fields passed to `OP_STRUCT_INIT`                                                                                                                                                                                                                                                                                                               |

---

## Usage Examples

### Variable Declaration

Code

```
var foo = "bar";
```

Bytecode

```
0000    1 OP_CONSTANT         1 'bar'
0002    | OP_DEFINE_GLOBAL    0 'foo'
0004    | OP_NIL
0005    | OP_RETURN
```

## Addition

Code

```
print 1 + 1;
```

Bytecode

```
0000    1 OP_CONSTANT         0 '1.000000'
0002    | OP_CONSTANT         1 '1.000000'
0004    | OP_ADD
0005    | OP_PRINT
0006    | OP_NIL
0007    | OP_RETURN
```
