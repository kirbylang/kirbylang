# Standard Library

The standard library is written in Kirby, in `stdlib/stdlib.krb`, and is part of
the `krb` binary. Everything here is available in every file. The functions
written in C are listed under [builtin functions](./builtin-functions.md).

## Traits

`Display`, `Eq`, `Ord`, `Default`, `Add`, `Sub`, `Mul`, `Div`, `Mod` and `Neg`.
See [builtin traits](../basics.md#builtin-traits).

## Methods On Primitive Types

| Type                      | Methods                                                                                                        |
| ------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `f64`, `f32`              | `abs`, `ceil`, `floor`, `max`, `min`, `pow`, `round`, `sqrt`, `trunc`                                          |
| `i8`, `i16`, `i32`, `i64` | `abs`, `max`, `min`                                                                                            |
| `u8`, `u16`, `u32`, `u64` | `max`, `min`                                                                                                   |
| `string`                  | `len`                                                                                                          |
| `[T]`                     | `len`, `isEmpty`, `push`, `pop`, `insert`, `remove`, `clear`, `contains`, `copy`, `reverse`, `slice`, `concat` |
| `[T; n]`                  | `len`, `contains`, `copy`, `reverse`, `slice`, `concat`, `toList`                                              |

## Traits On Primitive Types

| Type                          | Traits                                                               |
| ----------------------------- | -------------------------------------------------------------------- |
| every number type             | `Display`, `Eq`, `Ord`, `Default`, `Add`, `Sub`, `Mul`, `Div`, `Mod` |
| signed integers, `f64`, `f32` | `Neg`                                                                |
| `string`                      | `Display`, `Eq`, `Default`, `Add`                                    |
| `bool`                        | `Display`, `Eq`, `Default`                                           |
| `unit`                        | `Display`, `Eq`, `Ord`, `Default`                                    |

Lists and arrays implement no traits.

## Date

`Date` is a date and time in UTC. It implements `Display`, `Eq` and `Ord`.

```kirby
fun main(): unit {
    let date = Date.from_parts(2024, 3, 5, 1, 2, 3);

    @println(date); // 2024-03-05 01:02:03
    @println(date.get_year()); // 2024
    @println(date.get_timestamp()); // 1709600523
    @println(date > Date.new(0)); // true
}
```

| Static method                                       | Gives the date                                               |
| --------------------------------------------------- | ------------------------------------------------------------ |
| `Date.new(timestamp)`                               | of a number of seconds since 1970-01-01                      |
| `Date.now()`                                        | of the current time                                          |
| `Date.from_parts(year, month, day, hour, min, sec)` | made of these parts. A part that is out of range is an error |
| `Date.from(text)`                                   | written as `"2024-03-05 01:02:03"`                           |

The methods `get_timestamp`, `get_year`, `get_month`, `get_day`, `get_hour`,
`get_minute` and `get_second` each give an `i64`. The fields themselves are
private. `from_parts` accepts years from 1 to 9999.

`a.cmp(b)` on two dates gives the number of seconds from `b` to `a`.
