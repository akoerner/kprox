# KProx Script (.kps) Language Reference

KProx Script is a line-oriented scripting language that runs directly on the KProx device. Scripts are stored as `.kps` files on the SD card and executed via `{SD_EXEC /path/to/script.kps}` from any token string, the KPScript cardputer app, or the KPScript web editor.

Every `{TOKEN}` from the Token Reference works inside KPS string expressions. Variables are accessed as `${varname}`.

---

## File Conventions

- Extension: `.kps`
- Encoding: UTF-8, Unix or Windows line endings
- Recommended location: `/scripts/` on the SD card
- Comments: lines beginning with `#`

```kps
# This is a comment
set greeting "Hello World"
echo "${greeting}{ENTER}"
```

---

## Variables

```kps
set name value
set name "value with spaces"
set name ${other}
set n {MATH ${n} + 1}
unset name
```

`set` assigns a variable. The value is everything after the name, with leading/trailing whitespace trimmed. Quotes are stripped from the outer value if present. `${varname}` is substituted anywhere in any argument. `unset` removes a variable.

Variable names are case-sensitive. Variables persist for the lifetime of the script and are shared with any `include`d sub-scripts.

---

## String Expressions

Every argument that is not a block keyword is a string expression. Expressions support:

| Syntax | Behaviour |
|--------|-----------|
| `${varname}` | Variable substitution |
| `{TOKEN ...}` | Inline token evaluation (see Token Reference) |
| `"quoted string"` | Outer quotes stripped; inner `\"`, `\\`, `\n`, `\t` honoured |
| Bare word | Treated as a literal string (no whitespace splitting inside) |

```kps
set ip   {KPROX_IP}
set pass {CREDSTORE password mysite}
set code {TOTP mysite}
set sum  {MATH ${a} + ${b}}
set rand {RAND 1 100}
echo "ip=${ip} pass=${pass} code=${code}{ENTER}"
```

---

## Output — `echo` / `type`

```kps
echo expression
type expression
```

Evaluates the expression and sends the result to HID output (keyboard). `type` is an alias for `echo`. All `{TOKEN}` key sequences in the string are executed.

```kps
echo "admin{TAB}{CREDSTORE password mysite}{ENTER}"
echo "{CHORD CTRL+ALT+T}"
type "Hello World{ENTER}"
```

---

## Keys and Chords

```kps
key TOKEN_NAME
chord MODIFIER+KEY
```

`key` sends a single named key token. `chord` sends a modifier+key combination.

```kps
key ENTER
key TAB
key F5
key ESCAPE
chord CTRL+C
chord GUI+R
chord CTRL+ALT+DELETE
```

---

## Run Raw Token String

```kps
run "token_string"
```

Passes the evaluated string directly to the full token parser. Use when you need complex token sequences.

```kps
run "{CHORD CTRL+A}{SLEEP 200}{CHORD CTRL+C}"
run "{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP 500}{ENDLOOP}"
```

---

## Sleep

```kps
sleep milliseconds
```

Pauses execution. The server continues handling requests during the sleep.

```kps
sleep 500
sleep {MATH ${delay} * 2}
```

---

## Conditionals

```kps
if condition
    ...
elif condition
    ...
else
    ...
endif
```

Blocks nest correctly. `elif` and `else` are optional.

### Condition Operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Both sides are evaluated as expressions. Comparison is numeric when both sides parse as numbers; otherwise string comparison.

A bare expression with no operator is truthy when non-empty and not `"0"` or `"false"`.

```kps
if ${score} >= 90
    echo "A{ENTER}"
elif ${score} >= 75
    echo "B{ENTER}"
else
    echo "C or below{ENTER}"
endif

if {KPROX_IP} == ""
    echo "No WiFi{ENTER}"
endif

set ok 1
if ${ok}
    echo "truthy{ENTER}"
endif
```

---

## For Loop

```kps
for var start step end
    ...
endfor
```

Iterates `var` from `start` to `end` (inclusive) incrementing by `step`. Use a negative `step` to count down. `break` exits early.

```kps
for i 1 1 5
    echo "${i}{ENTER}"
endfor

for i 10 -2 0
    echo "${i} "
endfor
echo "{ENTER}"
```

---

## While Loop

```kps
while condition
    ...
endwhile
```

Repeats while `condition` is true. Evaluated before each iteration.

```kps
set n 5
while ${n} > 0
    set n {MATH ${n} - 1}
    echo "${n}{ENTER}"
endwhile
```

---

## Loop (Infinite or Timed)

```kps
loop
    ...
endloop

loop milliseconds
    ...
endloop
```

`loop` with no argument runs indefinitely until `break`, `return`, `halt`, or the user aborts (ESC / BtnA). `loop ms` exits automatically after `ms` milliseconds.

```kps
loop
    echo ".{SLEEP 500}"
    sleep 500
endloop

loop 10000
    chord CTRL+ALT+DELETE
    sleep 1000
endloop
```

---

## Break / Return

```kps
break
return
```

`break` exits the innermost `loop`, `while`, or `for` block. `return` exits the current script entirely (does not propagate to the caller when used inside `include`).

---

## Halt / Resume

```kps
halt
resume
```

`halt` stops all KProx execution (equivalent to the physical long-press halt). `resume` resumes a halted device.

---

## HID Output Routing

Selectively redirect output to specific transports within a script. Changes are scoped to the current `run` string or token sequence — the original state restores automatically when that string finishes.

```kps
run "{BLUETOOTH_HID false}Hello{ENTER}"
run "{USB_HID false}{BLUETOOTH_KEYBOARD true}secret{ENTER}"
run "{BLUETOOTH_MOUSE false}{SETMOUSE 400 300}{MOUSECLICK}"
```

| Token | Value | Effect |
|-------|-------|--------|
| `{BLUETOOTH_HID value}` | `true`/`false` | All BLE output on/off |
| `{BLUETOOTH_KEYBOARD value}` | `true`/`false` | BLE keyboard only |
| `{BLUETOOTH_MOUSE value}` | `true`/`false` | BLE mouse only |
| `{USB_HID value}` | `true`/`false` | All USB output on/off |
| `{USB_KEYBOARD value}` | `true`/`false` | USB keyboard only |
| `{USB_MOUSE value}` | `true`/`false` | USB mouse only |

These can also be used directly in KPS `echo` strings since `echo` passes through the full token parser:

```kps
# Type only over BLE — USB silent
echo "{BLUETOOTH_KEYBOARD true}{USB_KEYBOARD false}secret{ENTER}"
```

---

## Register Operations

```kps
play_register name_or_index
set_active_register name_or_index
```

`play_register` executes the token string in a named or 1-based indexed register. `set_active_register` changes the active register without executing it.

```kps
play_register "Endless Mouse"
play_register 2
set_active_register 1
```

---

## SD File Operations

```kps
sd_write  "/path/to/file.txt" "content"
sd_append "/path/to/file.txt" "more content"
include   "/path/to/sub.kps"
```

`sd_write` creates or overwrites a file. `sd_append` appends to a file (creates if absent). Both create parent directories automatically. `include` executes another `.kps` file and shares the current variable scope.

Reading a file is done via the `{SD_READ}` token in an expression:

```kps
set contents {SD_READ /config/profile.txt}
echo "${contents}"
```

```kps
sd_write  "/logs/run.log" "Started\n"
sd_append "/logs/run.log" "ip=${ip}\n"
include   "/scripts/helpers/login.kps"
```

---

## WiFi Connect

```kps
wifi_connect "SSID" "password"
```

Connects to a WiFi network. Equivalent to `{WIFI ssid password}`.

---

## Credential Store and TOTP

TOTP and credentials are accessed via inline tokens in expressions:

```kps
set pass {CREDSTORE password mysite}
set user {CREDSTORE username mysite}
set note {CREDSTORE notes mysite}
set code {TOTP mysite}

echo "${user}{TAB}${pass}{TAB}${code}{ENTER}"
```

The credential store must be unlocked for these to resolve. When locked they return an empty string.

---

## Device Info

```kps
set ip {KPROX_IP}
if ${ip} == ""
    echo "Not connected{ENTER}"
else
    echo "http://${ip}{ENTER}"
endif
```

---

## Full Example

```kps
# auto_login.kps — automated login with TOTP
# Place at /scripts/auto_login.kps

set site "github"
set user {CREDSTORE username ${site}}
set pass {CREDSTORE password ${site}}
set code {TOTP ${site}}

if ${user} == ""
    echo "CredStore locked — cannot log in{ENTER}"
    return
endif

# Focus browser and navigate
chord GUI+1
sleep 800
run "{CHORD CTRL+L}"
sleep 300
echo "https://github.com/login{ENTER}"
sleep 2000

# Fill form
echo "${user}{TAB}"
sleep 200
echo "${pass}{ENTER}"
sleep 2000

# 2FA
echo "${code}{ENTER}"
sleep 500
echo "Logged in as ${user}{ENTER}"
```

---

## Error Handling

KPS has no exception mechanism. If an operation fails silently (e.g. `{SD_READ}` on a missing file returns `""`), check the result:

```kps
set data {SD_READ /config/settings.txt}
if ${data} == ""
    echo "Config missing, using defaults{ENTER}"
    set data "default_value"
endif
```

---

## Execution Limits

| Limit | Value |
|-------|-------|
| Max script lines | 4096 |
| Max recursion depth | 16 (`include` + `play_register`) |
| Max expression iterations | 100 variable-substitution passes |
| Abort | ESC key or BtnA on Cardputer |
