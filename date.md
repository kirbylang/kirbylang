# 1. Major correctness issues

## 1. Negative timestamps are not handled correctly

This is the biggest issue.

```kirby
var days = ts / 86400;
let sod  = ts % 86400;
```

For negative `ts`, this is not enough.

You need **floor division** and **floor remainder** behavior, not the language’s default division/modulo behavior.

Example:

```kirby
Date.new(-1)
```

should represent:

```text
1969-12-31 23:59:59
```

But with the current code, depending on how your `%` operator behaves, you will likely get something like:

```text
1970-01-00 23:59:59
```

or

```text
1970-01-00 -1:-1:-1
```

Both are wrong.

The fix is to compute the day offset using floor division:

```kirby
var days = @floor(ts / 86400);
let sod  = @floor(ts - days * 86400);
```

Then handle negative `days` before converting to year/month/day.

For example:

```kirby
while (days < 0) {
    year = year - 1;
    days = days + Self.days_in_year(year);
}
```

where:

```kirby
fun days_in_year(year: f64): f64 = if (Self.is_leap(year)) 366 else 365;
```

Then also handle negative month offsets:

```kirby
while (days < 0) {
    month = month - 1;
    if (month == 0) {
        month = 12;
        year = year - 1;
    }
    days = days + Self.days_in_month(year, month);
}
```

Then the normal month/day logic can follow.

---

## 1a. The year loop only moves forward

This part only handles timestamps after the epoch:

```kirby
var year = 1970;

while (true) {
    let ydays = if (Self.is_leap(year)) 366 else 365;

    if (days < ydays) {
        break;
    }

    days = days - ydays;
    year = year + 1;
}
```

If `days` is negative, it breaks immediately and produces a bogus date in 1970.

You need either:

1. a backward loop for negative `days`, or
2. a full civil-date conversion algorithm that handles negative day offsets directly.

---

## 1b. The month loop assumes `days` is non-negative

Same issue:

```kirby
var month = 1;

while (true) {
    let mdays = Self.days_in_month(year, month);

    if (days < mdays) {
        break;
    }

    days = days - mdays;
    month = month + 1;
}
```

If `days` is negative, this produces invalid months/days.

Also, if `days` is too large because of an invalid date or fractional timestamp, `month` can become `13`, and then:

```kirby
lengths[month - 1]
```

will access index `12`, which is out of bounds for a 12-element array.

You should either:

- normalize/validate input dates before converting, or
- make `timestamp_to_date` defensive against out-of-range day offsets.

---

## 1c. Fractional timestamps make `timestamp` and fields inconsistent

Your `timestamp` is `f64`, so callers can do:

```kirby
Date.new(86399.9)
```

Then `timestamp_to_date` floors the fields:

```kirby
hour: @floor(hour),
minute: @floor(minute),
second: @floor(second),
```

so the visible date components may represent:

```text
1970-01-01 23:59:59
```

but the stored `timestamp` is still:

```text
86399.9
```

That means the date components are not canonical for the stored timestamp.

You should decide what `Date` represents:

### Option A: Whole seconds only

Then either:

- store `timestamp` as an integer type if your language has one, or
- floor `timestamp` at the boundary:

```kirby
let ts = @floor(timestamp);
```

and ensure all fields are integral.

### Option B: Fractional seconds are allowed

Then your struct needs subsecond precision, for example:

```kirby
pub let nanosecond: f64;
```

or you need to keep the timestamp as the source of truth and treat fields only as truncated components.

Right now, the implementation is ambiguous.

---

## 1d. `Date.from` does not validate or normalize calendar fields

This is a big issue.

Currently:

```kirby
let year: f64 = @parseNumber(year_str);
let month: f64 = @parseNumber(month_str);
let day: f64 = @parseNumber(day_str);

let hour: f64 = @parseNumber(hour_str);
let minute: f64 = @parseNumber(minute_str);
let second: f64 = @parseNumber(second_str);

Self {
    timestamp: Self.date_to_timestamp(year, month, day, hour, minute, second),
    year: year,
    month: month,
    day: day,
    hour: hour,
    minute: minute,
    second: second,
}
```

So `Date.from("2024-02-30 00:00:00")` will not fail.

It will compute a timestamp corresponding to `2024-03-01`, but the struct fields will still say:

```text
year = 2024
month = 2
day = 30
```

That is inconsistent.

You should either:

### Option A: validate strictly

Reject invalid dates:

```text
month: 1..12
day: 1..days_in_month(year, month)
hour: 0..23
minute: 0..59
second: 0..59
```

Example:

```kirby
@assert(year >= 1, "invalid year: @year");
@assert(month >= 1 and month <= 12, "invalid month: @month");
@assert(day >= 1 and day <= Self.days_in_month(year, month), "invalid day: @day");
@assert(hour >= 0 and hour <= 23, "invalid hour: @hour");
@assert(minute >= 0 and minute <= 59, "invalid minute: @minute");
@assert(second >= 0 and second <= 59, "invalid second: @second");
```

### Option B: normalize automatically

Parse the fields, compute the timestamp, then convert the timestamp back into canonical fields:

```kirby
let timestamp = Self.date_to_timestamp(year, month, day, hour, minute, second);
let normalized = Self.timestamp_to_date(timestamp);
```

Then return the normalized date.

That way:

```kirby
Date.from("2024-02-30 00:00:00")
```

could become:

```text
2024-03-01 00:00:00
```

and all fields would match the timestamp.

For a date library, I would recommend **strict validation by default**, because silent normalization can hide bugs.

---

## 1e. `Date.from` does not enforce the exact format

Right now it only checks:

- there are 2 space-separated parts,
- the date part has 3 dash-separated parts,
- the time part has 3 colon-separated parts.

So these are accepted as long as they parse as numbers:

```text
2024-1-2 3:4:5
-12345-99-99 99:99:99
abc-def-ghi h:m:s
```

depending on how your `@parseNumber` behaves.

If you want ISO-like input, enforce:

- year length maybe 4 digits, or allow longer years but require sign rules,
- month `01`-`12`,
- day `01`-`31`,
- hour `00`-`23`,
- minute `00`-`59`,
- second `00`-`59`,
- exactly one space separator,
- no leading/trailing whitespace,
- digits only for these components,
- no timezone suffix.

Example:

```kirby
@assert(value == @trim(value), "input must not have surrounding whitespace");
@assert(!@strContains(value, "  "), "input must use a single space separator");
```

Then validate each component.

---

## 1f. `timestamp_to_date` may be very slow for large timestamps

The current implementation uses loops:

```kirby
while (true) {
    let ydays = if (Self.is_leap(year)) 366 else 365;
    ...
    year = year + 1;
}
```

That means converting a date 1,000,000 years in the future can require about 1,000,000 iterations.

For most practical timestamps this may be acceptable, but it is not ideal.

You already use Howard Hinnant-style math in `date_from_civil`, so you can use the inverse algorithm in `timestamp_to_date`.

That would make conversion O(1) instead of O(number of years/months).

A rough structure:

```kirby
fun timestamp_to_date(ts: f64): Self {
    let days = @floor(ts / 86400);
    let sod = @floor(ts - days * 86400);

    let hour   = @floor(sod / 3600);
    let minute = @floor((sod - hour * 3600) / 60);
    let second = @floor(sod - hour * 3600 - minute * 60);

    let (year, month, day) = Self.civil_from_days(days + 719468);

    return Self {
        timestamp: ts,
        year: year,
        month: month,
        day: day,
        hour: hour,
        minute: minute,
        second: second,
    };
}
```

where `civil_from_days` is the inverse of your `date_from_civil` algorithm.

This is much cleaner and handles pre-1970 dates if implemented correctly.

---

# 2. Calendar math issues

## 2.1 `date_from_civil` has a negative-year problem

This part:

```kirby
let era = @floor(
    if (year >= 0) year / 400
    else (year - 399) / 400
);
```

is problematic.

The `year - 399` trick is usually meant to be used with **truncating integer division**, not with floating-point division followed by `@floor`.

With floating-point semantics, for negative years at certain boundaries, this can be wrong.

If `year / 400` is ordinary floating-point division, the safer version is:

```kirby
let era = @floor(year / 400);
```

provided the values are exact enough.

Alternatively, if your language has integer division or you want explicit integer-like behavior, define a floor-division helper:

```kirby
fun floor_div(a: f64, b: f64): f64 {
    let q = a / b;
    return if (q == @floor(q)) q else @floor(q);
}
```

Then:

```kirby
let era = Self.floor_div(year, 400);
```

For positive years near 1970 onward this is probably not visible, but if you intend to support negative years/BCE, it matters.

---

## 2.2 `days_in_month` does not validate `month`

```kirby
return lengths[month - 1];
```

If `month` is `0`, `13`, `2.5`, `NaN`, or otherwise out of range, this is undefined or wrong.

You should guard it:

```kirby
@assert(month >= 1 and month <= 12, "invalid month: @month");
```

or return an error/optional instead of panicking.

---

## 2.3 Leap-year math with `f64` is risky

This is fine for small integer years:

```kirby
year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
```

But because `year` is `f64`, you are relying on floating-point exactness.

For years within the normal calendar range this is okay, but for very large years it becomes risky.

If your language has integer types, calendar fields should be integers:

```kirby
year: i64
month: i32
day: i32
hour: i32
minute: i32
second: i32
timestamp: i64
```

If your language only has `f64`, then you should explicitly floor all fields and document that `Date` stores whole seconds only.

---

# 3. String formatting issues

## 3.1 `@numberToString` formatting must be reliable

Your `toString` uses:

```kirby
Date.pad_zeros(self.month, 2)
```

and `pad_zeros` uses:

```kirby
var str = @numberToString(num);
```

This only works if `@numberToString(2)` produces `"2"`, not `"2.0"`.

If `@numberToString` produces `"2.0"`, then:

```kirby
Date.pad_zeros(2, 2)
```

would return `"2.0"`, and the date string would become:

```text
1970-2.0-01 00:00:00
```

or something similar.

Your test passes, so your language probably prints integral `f64` values without a decimal part. But this is fragile.

Consider a dedicated integer formatter:

```kirby
fun format_int(n: f64): string {
    @assert(n == @floor(n), "expected integer: @n");
    return @numberToString(@floor(n));
}
```

or use an integer type if available.

---

## 3.2 `pad_zeros` does not handle negative values well

For example:

```kirby
Date.pad_zeros(-1, 2)
```

probably returns `"-1"`, because `@len("-1")` is already `2`.

If calendar fields should never be negative except maybe year, that may be okay. But it is not a general-purpose zero-padding function.

If you want signed padding, you need something like:

```kirby
fun pad_zeros(num: f64, length: f64): string {
    let negative = num < 0;
    let abs = if (negative) -num else num;
    var str = @numberToString(abs);

    while (@len(str) < length) {
        str = "0" + str;
    }

    return if (negative) "-" + str else str;
}
```

But for date components other than year, negative values should be invalid.

---

## 3.3 Year formatting is ambiguous

Your format:

```text
{self.year}-{month}-{day} {hour}:{minute}:{second}
```

has no fixed year width.

That is fine for modern years, but for:

```text
-44 BC
0
999
10000
```

the format becomes ambiguous or non-ISO.

If you want an ISO-like format, decide whether to support:

```text
-0004-01-01 00:00:00
+0004-01-01 00:00:00
0004-01-01 00:00:00
```

and document the sign convention.

If you only support AD 1 onward, validate and enforce that.

---

# 4. Equality and ordering issues

## 4.1 `Ord` using subtraction is not ideal

```kirby
fun cmp(self, other: Self): f64 = self.timestamp - other.timestamp;
```

This works if your `Ord` trait expects any negative/zero/positive number.

But subtraction has problems:

- If timestamps are very close but large, floating-point subtraction may lose precision.
- If one timestamp is huge and the other is slightly larger, the sign should still be correct, but direct comparison is safer.
- If timestamps can be `NaN`, subtraction can produce `NaN`, which is not a valid comparison result.

Prefer:

```kirby
fun cmp(self, other: Self): f64 {
    if (self.timestamp < other.timestamp) {
        return -1;
    } else if (self.timestamp > other.timestamp) {
        return 1;
    } else {
        return 0;
    }
}
```

If your `Ord` trait allows returning a score like Rust’s `Ord::cmp`, this is cleaner.

---

## 4.2 `Eq` only compares timestamps

```kirby
fun equals(self, other: Self): bool = self.timestamp == other.timestamp;
```

This is fine if every `Date` value is canonical.

But because `Date.from` currently stores unvalidated fields, two values can have:

```text
same timestamp
different invalid fields
```

and `equals` will still say they are equal.

Example conceptually:

```text
timestamp = 2024-03-01 00:00:00
fields    = 2024-02-30 00:00:00
```

and

```text
timestamp = 2024-03-01 00:00:00
fields    = 2024-03-01 00:00:00
```

would compare equal by timestamp, but their printed forms differ.

If you validate/normalize all constructors, timestamp equality is usually sufficient.

If you do not, you should compare all fields:

```kirby
fun equals(self, other: Self): bool =
    self.timestamp == other.timestamp and
    self.year == other.year and
    self.month == other.month and
    self.day == other.day and
    self.hour == other.hour and
    self.minute == other.minute and
    self.second == other.second;
```

But I would rather make the invariant true: **all `Date` values are canonical**.

---

# 5. Time zone / UTC / DST issues

Your timestamp math assumes:

```text
seconds since 1970-01-01 00:00:00 UTC
```

There is no DST handling, which is correct for UTC.

But then you must document what `Date` represents.

If `@now()` returns a local-time epoch, or if you expect users to construct dates in local time, you will get wrong results.

You should explicitly say:

```text
Date is a UTC calendar timestamp. It does not store timezone information.
```

If you want local dates, you need either:

- a separate `DateTime` type with timezone, or
- explicit UTC/local constructors.

Example:

```kirby
Date.from_utc("2024-02-30 00:00:00")
Date.from_local("2024-02-30 00:00:00")
```

Otherwise `Date.from` is ambiguous.

---

# 6. Recommended invariants

Your `Date` type should enforce this invariant:

> A valid `Date` stores a Unix timestamp and canonical calendar fields that exactly match that timestamp in the proleptic Gregorian calendar, UTC, with whole seconds only.

If that invariant is true, then:

- `toString()` is predictable,
- `equals` by timestamp is okay,
- `cmp` by timestamp is okay,
- `from()` should validate or normalize,
- `new()` should floor or reject fractional timestamps,
- fields should be integral.

---

# 7. Suggested improvements

## 7.1 Use integers if your language has them

If available, prefer:

```kirby
struct Date {
    pub let timestamp: i64;

    pub let year: i32;
    pub let month: i32;
    pub let day: i32;
    pub let hour: i32;
    pub let minute: i32;
    pub let second: i32;
}
```

This avoids most floating-point precision and formatting issues.

If your language only has `f64`, then at least floor everything at the public boundary.

---

## 7.2 Validate input in `Date.from`

A more robust version could look like this:

```kirby
pub fun from(value: string): Self {
    let parts: Array = @strSplit(value, " ");

    @assert(@len(parts) == 2, $"invalid input string. expected 2 parts but got {@numberToString(@len(parts))}");

    let date_part: string = parts[0];
    let time_part: string = parts[1];

    let date_parts = @strSplit(date_part, "-");
    @assert(@len(date_parts) == 3, $"invalid input string. expected 3 date parts but got {@numberToString(@len(date_parts))}");

    let time_parts = @strSplit(time_part, ":");
    @assert(@len(time_parts) == 3, $"invalid input string. expected 3 time parts but got {@numberToString(@len(time_parts))}");

    let year: f64 = @parseNumber(date_parts[0]);
    let month: f64 = @parseNumber(date_parts[1]);
    let day: f64 = @parseNumber(date_parts[2]);

    let hour: f64 = @parseNumber(time_parts[0]);
    let minute: f64 = @parseNumber(time_parts[1]);
    let second: f64 = @parseNumber(time_parts[2]);

    @assert(year == @floor(year), "year must be an integer");
    @assert(month == @floor(month), "month must be an integer");
    @assert(day == @floor(day), "day must be an integer");
    @assert(hour == @floor(hour), "hour must be an integer");
    @assert(minute == @floor(minute), "minute must be an integer");
    @assert(second == @floor(second), "second must be an integer");

    @assert(year >= 1, "year must be at least 1");
    @assert(month >= 1 and month <= 12, "month must be in 1..12");
    @assert(day >= 1 and day <= Self.days_in_month(year, month), "day is out of range for year/month");
    @assert(hour >= 0 and hour <= 23, "hour must be in 0..23");
    @assert(minute >= 0 and minute <= 59, "minute must be in 0..59");
    @assert(second >= 0 and second <= 59, "second must be in 0..59");

    let timestamp = Self.date_to_timestamp(year, month, day, hour, minute, second);

    return Self {
        timestamp: timestamp,
        year: year,
        month: month,
        day: day,
        hour: hour,
        minute: minute,
        second: second,
    };
}
```

If you want to support negative years, adjust the `year >= 1` rule.

If you want automatic normalization, replace validation with:

```kirby
let timestamp = Self.date_to_timestamp(year, month, day, hour, minute, second);
return Self.timestamp_to_date(timestamp);
```

but then document that invalid components are normalized.

---

## 7.3 Fix `timestamp_to_date`

A safer version, assuming you add `days_in_year`:

```kirby
fun days_in_year(year: f64): f64 {
    return if (Self.is_leap(year)) 366 else 365;
}

fun timestamp_to_date(ts: f64): Self {
    let ts = @floor(ts);

    var days = @floor(ts / 86400);
    let sod = ts - days * 86400;

    let hour   = @floor(sod / 3600);
    let minute = @floor((sod - hour * 3600) / 60);
    let second = sod - hour * 3600 - minute * 60;

    var year = 1970;

    while (days < 0) {
        year = year - 1;
        days = days + Self.days_in_year(year);
    }

    while (days >= Self.days_in_year(year)) {
        days = days - Self.days_in_year(year);
        year = year + 1;
    }

    var month = 1;

    while (days < 0) {
        month = month - 1;
        if (month == 0) {
            month = 12;
            year = year - 1;
        }
        days = days + Self.days_in_month(year, month);
    }

    while (days >= Self.days_in_month(year, month)) {
        days = days - Self.days_in_month(year, month);
        month = month + 1;
        if (month == 13) {
            month = 1;
            year = year + 1;
        }
    }

    let day = days + 1;

    return Self {
        timestamp: ts,
        year: year,
        month: month,
        day: day,
        hour: hour,
        minute: minute,
        second: second,
    };
}
```

This still uses loops, but it at least handles negative day offsets.

For performance, replace the loops with the inverse Hinnant civil-date algorithm.

---

## 7.4 Fix negative-year era calculation

If you support negative years, use:

```kirby
let era = @floor(year / 400);
```

or implement proper floor division.

Do not mix the `year - 399` trick with floating-point `@floor` unless you have verified it for all boundary cases.

---

## 7.5 Make `toString` robust

For integral components:

```kirby
fun pad2(num: f64): string {
    @assert(num >= 0 and num <= 99, "expected 0..99: @num");
    let str = @numberToString(@floor(num));
    if (@len(str) == 1) {
        return "0" + str;
    }
    return str;
}

fun toString(self): string {
    let month = Date.pad2(self.month);
    let day = Date.pad2(self.day);
    let hour = Date.pad2(self.hour);
    let minute = Date.pad2(self.minute);
    let second = Date.pad2(self.second);

    return $"{self.year}-{month}-{day} {hour}:{minute}:{second}";
}
```

If year can be negative, decide on the format:

```text
-44-01-01 00:00:00
```

or fixed width:

```text
-0044-01-01 00:00:00
```

---

# 8. Suggested test cases

Your current test is good, but not enough.

Add tests like:

```kirby
// Epoch
@assert(Date.new(0).toString() == "1970-01-01 00:00:00");

// One second before epoch
@assert(Date.new(-1).toString() == "1969-12-31 23:59:59");

// One second after epoch
@assert(Date.new(1).toString() == "1970-01-01 00:00:01");

// One day before epoch
@assert(Date.new(-86400).toString() == "1969-12-31 00:00:00");

// Leap day
@assert(Date.from("1972-02-29 00:00:00").toString() == "1972-02-29 00:00:00");

// Non-leap February 29 should fail or normalize
// Option A: strict validation
// @assert(!Date.tryFrom("2023-02-29 00:00:00").ok);

// Invalid day should fail or normalize
// @assert(!Date.tryFrom("2024-02-30 00:00:00").ok);

// Round-trip
let d = Date.from("2024-02-28 23:59:59");
@assert(d.toString() == "2024-02-28 23:59:59");

// Ordering
let a = Date.from("2024-01-01 00:00:00");
let b = Date.from("2023-12-31 23:59:59");
@assert(b < a);
@assert(a > b);

// Equality
let x = Date.from("2024-01-01 00:00:00");
let y = Date.from("2024-01-01 00:00:00");
@assert(x.equals(y));
```

Also test leap-year boundaries:

```text
1900-02-28 23:59:59
1900-03-01 00:00:00
2000-02-28 23:59:59
2000-02-29 23:59:59
2000-03-01 00:00:00
2400-02-28 23:59:59
2400-02-29 23:59:59
```

And far dates:

```text
2100-02-28 00:00:00
2100-03-01 00:00:00
```

Because 2100 is not a leap year.

---

# 9. Specific notes on your existing code

## 9.1 `pub let timestamp: f64` is a public mutable source of truth

It is immutable because of `let`, but it is public. That means users can read the timestamp directly. That is okay.

But because fields are also public, users can compare:

```kirby
date.timestamp
date.year
date.month
date.day
```

If those are ever inconsistent, users will notice.

So the invariant is very important.

---

## 9.2 `Date.now()` depends on `@now()`

You should document what `@now()` returns:

- Unix seconds?
- Unix milliseconds?
- Local time?
- UTC time?
- Fractional seconds?

If `@now()` returns milliseconds, your date will be wildly wrong.

If it returns fractional seconds, decide whether to truncate or store subseconds.

---

## 9.3 `is_leap` and `days_in_month` are private, but useful for validation

Since `Date.from` is public and needs validation, either:

- make those helpers public,
- or perform validation inside `Date.from` only.

If other user code needs `days_in_month`, expose it:

```kirby
pub fun days_in_month(year: f64, month: f64): f64 { ... }
```

But if it panics/asserts on invalid input, make that behavior explicit.

---

## 9.4 The `Date.from` debug println should probably be removed

```kirby
@println($"DEBUG: date_part: {date_part}; time_part: {time_part}");
```

This is noisy.

If you want debug logging, gate it behind a flag:

```kirby
if (@debug_enabled()) {
    @println(...);
}
```

or remove it from the final implementation.

---

# 10. Overall assessment

Your implementation is a good prototype.

The core algorithm is mostly correct for positive integer timestamps and valid dates.

The main problems are:

1. **Negative timestamps are broken.**
2. **Invalid calendar/time components are accepted.**
3. **`Date.from` can create non-canonical `Date` values.**
4. **Floating-point fields are risky for calendar math.**
5. **The year/month conversion loops are slow for far-away dates.**
6. **Negative-year civil math is not fully correct.**
7. **Equality/ordering should compare timestamps directly, not by subtraction.**
8. **Time zone/UTC semantics need documentation.**

If I had to rank the fixes:

### Must fix

- Floor division/remainder for timestamp conversion.
- Handle negative days before the epoch.
- Validate or normalize fields in `Date.from`.
- Decide on whole seconds vs fractional seconds.
- Document UTC semantics.

### Should fix

- Use integer types if available.
- Replace loops with O(1) civil-date math.
- Fix negative-year era calculation.
- Use direct comparison in `Ord`.
- Make `toString` formatting robust.

### Nice to have

- Strict ISO parsing.
- Optional/Result return types instead of asserting.
- Subsecond support.
- Timezone-aware types.
- More comprehensive tests.

---

# 11. Recommended final design

If your language has integers, I would define:

```kirby
struct Date {
    pub let timestamp: i64;

    pub let year: i32;
    pub let month: i32;
    pub let day: i32;
    pub let hour: i32;
    pub let minute: i32;
    pub let second: i32;
}
```

If it does not, keep `f64` but enforce:

```text
timestamp is an integer number of seconds
all fields are integer-valued
Date values are canonical
```

Then implement:

```kirby
pub fun new(timestamp: f64): Self
pub fun now(): Self
pub fun from(value: string): Self
fun from_parts(year: f64, month: f64, day: f64, hour: f64, minute: f64, second: f64): Self
```

with one shared normalization/validation path.

For example:

```kirby
fun from_parts(year: f64, month: f64, day: f64, hour: f64, minute: f64, second: f64): Self {
    @assert(year == @floor(year), "year must be integral");
    @assert(month == @floor(month), "month must be integral");
    @assert(day == @floor(day), "day must be integral");
    @assert(hour == @floor(hour), "hour must be integral");
    @assert(minute == @floor(minute), "minute must be integral");
    @assert(second == @floor(second), "second must be integral");

    @assert(month >= 1 and month <= 12, "month out of range");
    @assert(day >= 1 and day <= Self.days_in_month(year, month), "day out of range");
    @assert(hour >= 0 and hour <= 23, "hour out of range");
    @assert(minute >= 0 and minute <= 59, "minute out of range");
    @assert(second >= 0 and second <= 59, "second out of range");

    let timestamp = Self.date_to_timestamp(year, month, day, hour, minute, second);

    return Self {
        timestamp: timestamp,
        year: year,
        month: month,
        day: day,
        hour: hour,
        minute: minute,
        second: second,
    };
}
```

Then:

```kirby
pub fun from(value: string): Self {
    // parse fields
    return Self.from_parts(year, month, day, hour, minute, second);
}
```

And `new` can either:

```kirby
let ts = @floor(timestamp);
return Self.timestamp_to_date(ts);
```

or reject non-integral timestamps.

---

# 12. Short version

Your code is close, but right now it is only safe for:

```text
timestamp >= 0
timestamp is an integer number of seconds
Date.from input is valid and canonical
year is positive
```

For a robust `Date` type, I would make these changes:

```text
1. Use floor division for day/sod extraction.
2. Support negative days in timestamp_to_date.
3. Validate or normalize Date.from input.
4. Make all Date values canonical.
5. Use direct timestamp comparison in Ord.
6. Document UTC/proleptic Gregorian/whole-second semantics.
7. Prefer integer types for calendar fields if available.
```

With those fixes, your `Date` implementation will be much stronger.
