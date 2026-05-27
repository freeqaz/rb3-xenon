# Fixable Patterns: Handler Macros

Patterns related to Milo engine handler macros (`HANDLE_ACTION`, `HANDLE_EXPR`, `HANDLE`, etc.) inside `BEGIN_HANDLERS`/`END_HANDLERS` blocks.

---

## Manual Handler Extraction

**Impact:** +3-5%
**Success Rate:** HIGH (when method inlines)
**Time:** 5 minutes

Replace manual `_NEW_STATIC_SYMBOL` + `if (sym == _s)` handler blocks with standard `HANDLE_*` macros by extracting the handler body into a method.

### Symptom

Inside a `BEGIN_HANDLERS`/`END_HANDLERS` block, you see a manual handler expansion instead of a standard macro:

```cpp
BEGIN_HANDLERS(MyClass)
    {
        static Symbol _s("do_thing");  // or _NEW_STATIC_SYMBOL(do_thing)
        if (sym == _s) {
            int idx = _msg->Int(2);
            LocalUser *user = _msg->Obj<LocalUser>(3);
            DataArray *script = mItems[idx].mScript;
            if (script)
                CallScript(script, user);
            return 0;
        }
    }
    HANDLE_ACTION(other, OtherAction())
END_HANDLERS
```

objdiff may show extra instructions from redundant address computations, different inlining decisions, or suboptimal register usage compared to the macro-based version.

### Why It Works

The standard `HANDLE_ACTION` macro expands to the same `static Symbol` + `if (sym == _s)` + `return 0` pattern, but routes through a method call. When MSVC PPC `/O1` inlines the method, it often produces tighter codegen than the manual expansion because:

1. **Method inlining gives the compiler a cleaner IR** — the optimizer sees a single call site with well-typed parameters rather than inline code mixed with macro variables.
2. **MSVC right-to-left argument evaluation** naturally produces the correct register assignment order for `_msg` accessor calls without manual variable extraction.
3. **Eliminates redundant code** — if the logic already exists as a method (e.g., `Clear()`), the manual handler duplicates it, and the duplicate may compile differently.

### Fix

Extract the handler body into a method, then use the appropriate `HANDLE_*` macro:

```cpp
// Define the method (small enough to inline)
void MyClass::DoThing(int idx, LocalUser *user) {
    DataArray *script = mItems[idx].mScript;
    if (script)
        CallScript(script, user);
}

// Use standard macro — MSVC evaluates Obj<LocalUser>(3) before Int(2)
BEGIN_HANDLERS(MyClass)
    HANDLE_ACTION(do_thing, DoThing(_msg->Int(2), _msg->Obj<LocalUser>(3)))
    HANDLE_ACTION(other, OtherAction())
END_HANDLERS
```

### Choosing the Right Macro

| Handler returns | Macro | Example |
|----------------|-------|---------|
| `return 0` | `HANDLE_ACTION` | `HANDLE_ACTION(clear, Clear())` |
| A value | `HANDLE_EXPR` | `HANDLE_EXPR(toggle, ToggleFeature())` |
| `_HANDLE_CHECKED` result | `HANDLE` | `HANDLE(cheat_invoked, OnCheatInvoked)` |

### Real Examples

| Function | Before | After | Delta | Change |
|----------|--------|-------|-------|--------|
| CheatProvider::Handle | 96.0% | 99.8% | +3.8% | Extracted `Invoke()`, used `HANDLE_ACTION` |
| CharDriver::Handle | 93.3% | 98.4% | +5.1% | Extracted `AddBeat()`, reused `Clear()` |
| StorePanel::Handle | 96.5% | 99.5% | +3.0% | Extracted `SetSource()` |
| Automator::Handle | 98.8% | 98.8% | 0% | Manual `_HANDLE_CHECKED` → `HANDLE` (readability only) |

### Detection

Look for these patterns inside `BEGIN_HANDLERS`/`END_HANDLERS`:

```
static Symbol _s("...");
if (sym == _s) {
    ...
    return 0;  // or return <value>;
}
```

Or:
```
_NEW_STATIC_SYMBOL(name)
if (sym == _s) {
    ...
}
```

### Constraints

- **Method must be small enough to inline** under MSVC PPC `/O1`. A few lines is safe; 20+ lines probably won't inline and will produce worse codegen (out-of-line call overhead).
- **Don't extract handlers with `MILO_ASSERT`** — moving `MILO_ASSERT` to a different method changes `__LINE__`, breaking the assert line number match. The MoveDir.cpp handlers (which use `MILO_ASSERT`) are already 100% and should be left as-is.
- **Verify with objdiff after each change** — inlining is not guaranteed. If the match gets worse, the method was too large to inline.
- **MSVC right-to-left evaluation** is key — `HANDLE_ACTION(foo, Method(_msg->Int(2), _msg->Obj<Bar>(3)))` evaluates `Obj<Bar>(3)` first, then `Int(2)`. This often matches the target's register order naturally, but verify.

### Reusing Existing Methods

Before creating a new extraction method, check if one already exists:

```cpp
// Bad: manual handler duplicates Clear()
{
    static Symbol _s("clear");
    if (sym == _s) {
        if (mFirst)
            mFirst->DeleteStack();
        mFirst = nullptr;
        return 0;
    }
}

// Good: reuse existing Clear() method
HANDLE_ACTION(clear, Clear())
```

This was the case for `CharDriver::Clear()` — the manual handler was identical to the existing method, and using `HANDLE_ACTION(clear, Clear())` improved the match from 93.3% to 98.4%.

---

## Static Symbol Declaration Order in Handlers

**Impact:** +0.2-1%
**Success Rate:** MEDIUM
**Time:** 2 minutes

See also: [fixable-declarations.md](fixable-declarations.md#static-symbol-order)

When a handler function has multiple `static Symbol` declarations (e.g., in `ApplyFilter`), their declaration order affects callee-saved register allocation for the guard variables.

### Symptom

Register swap on callee-saved registers (r19↔r20, etc.) in a function with multiple static Symbol locals.

### Fix

Reorder `static Symbol` declarations to match the target's initialization order. Determine the target order from objdiff's static guard variable addresses (`$S1`, `$S2`, etc.).

### Real Example

```cpp
// Before (r19↔r20 regswap):
static Symbol all("all");
static Symbol modes("modes");
static Symbol filters("filters");

// After (fixed):
static Symbol all("all");
static Symbol filters("filters");
static Symbol modes("modes");
```

CheatProvider::ApplyFilter: 99.4% → 99.6%.
