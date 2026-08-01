# Bytecode

---

## Opcode Table

| Opcode                  | Operand(s)                         | Stack | Notes                                                                |
| ----------------------- | ---------------------------------- | ----: | -------------------------------------------------------------------- |
| **OP_CONSTANT**         | `index`                            |    +1 |                                                                      |
| **OP_NIL**              |                                    |    +1 |                                                                      |
| **OP_TRUE**             |                                    |    +1 |                                                                      |
| **OP_FALSE**            |                                    |    +1 |                                                                      |
| **OP_ADD**              |                                    | -2 +1 |                                                                      |
| **OP_SUBTRACT**         |                                    | -2 +1 |                                                                      |
| **OP_MULTIPLY**         |                                    | -2 +1 |                                                                      |
| **OP_DIVIDE**           |                                    | -2 +1 |                                                                      |
| **OP_MODULO**           |                                    | -2 +1 |                                                                      |
| **OP_NEGATE**           |                                    | -1 +1 |                                                                      |
| **OP_NOT**              |                                    | -1 +1 |                                                                      |
| **OP_PRINT**            |                                    |       |                                                                      |
| **OP_RETURN**           |                                    |       |                                                                      |
| **OP_EQUAL**            |                                    | -2 +1 |                                                                      |
| **OP_GREATER**          |                                    | -2 +1 |                                                                      |
| **OP_LESS**             |                                    | -2 +1 |                                                                      |
| **OP_POP**              |                                    |    -1 |                                                                      |
| **OP_DEFINE_GLOBAL**    | `index`                            |       |                                                                      |
| **OP_GET_GLOBAL**       | `index`                            |       |                                                                      |
| **OP_SET_GLOBAL**       | `index`                            |       |                                                                      |
| **OP_GET_LOCAL**        | `slot`                             |       |                                                                      |
| **OP_SET_LOCAL**        | `slot`                             |       |                                                                      |
| **OP_JUMP_IF_FALSE**    | `jumpOffset`                       |       |                                                                      |
| **OP_JUMP_IF_NOT_NIL**  | `jumpOffset`                       |       |                                                                      |
| **OP_JUMP**             | `jumpOffset`                       |       |                                                                      |
| **OP_LOOP**             | `jumpOffset`                       |       |                                                                      |
| **OP_CLOSE_UPVALUE**    |                                    |       |                                                                      |
| **OP_GET_UPVALUE**      | `slot`                             |       |                                                                      |
| **OP_SET_UPVALUE**      | `slot`                             |       |                                                                      |
| **OP_GET_PROPERTY**     | `index`                            |       |                                                                      |
| **OP_SET_PROPERTY**     | `index`, `index`                   |       |                                                                      |
| **OP_STRUCT**           | `index`                            |       |                                                                      |
| **OP_STRUCT_INIT**      | f                                  |       |                                                                      |
| **OP_METHOD**           | `index`                            |       |                                                                      |
| **OP_INVOKE**           | `index`, `argCount`                |       |                                                                      |
| **OP_CALL**             | `argCount`                         |       |                                                                      |
| **OP_CLOSURE**          | `index`, `isLocal`, `upvalueIndex` |       |                                                                      |
| **OP_FIELD**            | `index`                            |       |                                                                      |
| **OP_ARRAY**            | `itemCount`                        | -n +1 |                                                                      |
| **OP_INDEX_GET**        |                                    | -2 +1 |                                                                      |
| **OP_INDEX_SET**        |                                    | -3 +1 |                                                                      |
| **OP_CLOSE_BLOCK_EXPR** | `upvalueCount`                     | -n +1 | Cleans upvalues while leaving block expression return value on stack |

### Operand Encodings

| Operand type     | Size       | Description                                                                                                                                          |
| ---------------- | ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **index**        | `uint8_t`  | Index into the chunk’s constant table.                                                                                                               |
| **slot**         | `uint8_t`  | Slot number in the current call frame (local or upvalue).                                                                                            |
| **argCount**     | `uint8_t`  | For `OP_CLOSURE` only: tells whether the captured variable lives in the immediate surrounding function (`1`) or is itself an upvalue (`0`).          |
| **itemCount**    | `uint8_t`  | For `OP_ARRAY` only: determines how many values on the stack go in to the array                                                                      |
| **upvalueCount** | `uint8_t`  | For `OP_CLOSE_BLOCK_EXPR` only: determines how many upvalues to discard                                                                              |
| **isLocal**      | `uint8_t`  | (0 or 1) For `OP_CLOSURE` only: tells whether the captured variable lives in the immediate surrounding function (`1`) or is itself an upvalue (`0`). |
| **upvalueIndex** | `uint8_t`  | Index of the local or upvalue being captured (used together with the byte above).                                                                    |
| **jumpOffset**   | `uint16_t` | Relative jump distance measured from the **next instruction** after the offset field. (big‑endian: high byte first)                                  |

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
