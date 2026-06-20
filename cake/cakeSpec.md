<!-- SPDX-License-Identifier: GPL-2.0-only -->

# !^CAKE Language Specification

Version 0.1.0 (draft)

## Overview

!^CAKE (pronounced un-carrot cake (deliberately meaningless)) is a schema definition language inspired by the fictional ^CAKE language from Homestuck. !^CAKE is about baking. A recipe describes the exact byte layout of a C-compatible value; recipes are combined with the alchemization operators `&&` and `||`; and every recipe deterministically reduces to an 8-character captchalogue code that two peers can compare, over a wire, before exchanging payload data.

!^CAKE exists to fill a specific hole in !~ATH. The !~ATH foreign-session FFI marshals scalars and pointers but cannot describe an aggregate, so it cannot pass or return a `struct` by value. A recipe is exactly the missing description: feed one to a `TRANSCRIBE` and the runtime can build the `ffi_type` element array that lets a struct cross the universe boundary intact.

!^CAKE is purely declarative. A `.^CAKE` file contains recipes. `.^CAKE` files do not contain anything that executes. Recipes are eternal, unlike the mortality theme of !~ATH. The verbs that actually bake, fill, and read a buffer (`BAKE`, `SPRINKLE`, `SCOOP`, ...) live on the !~ATH side, as built-in rites that operate on a recipe once a `.^CAKE` file has been imported as a module.

> Status: nothing is implemented yet. This document specifies the language; the two things it pins down exactly -- the layout algorithm and the captchalogue-code canonicalization -- are the parts that every implementation must reproduce bit-for-bit, because a code is only useful if two independent implementations agree on it.

---

## Lexical Structure

### Files

!^CAKE source files are recommended to use the file extension `.^CAKE` (e.g., `shapes.^CAKE`). Files are UTF-8 encoded.

### Comments

Single-line comments only, identical to !~ATH:

```
// This is a comment
```

### Identifiers

Identifiers begin with a letter or underscore, followed by letters, digits, or underscores. Identifiers are case-sensitive.

```
Point
myRecipe
_reserved
field2
```

The bare identifier `_` (a single underscore) is special: it names a reserved ingredient (padding) that occupies space but cannot be read or written. See *Reserved Ingredients*.

### Keywords

Reserved words (cannot be used as recipe names, ingredient names, or measure names):

```
// declarations
RECIPE INGREDIENT MEASURE PUNCHED

// layout modifiers / directives
DENSE IMPERIAL RISE TO OF CRUST

// scalar ingredient types (fixed-width)
PINCH DASH SPOON CUP DROP DOLLOP SIGNED

// scalar ingredient types (native / FFI)
INTEGER BOOLEAN STRING RELIC

// implicit union field
FLAVOR
```

The alchemization operators `&&` and `||` are punctuation, not keywords, but they may only appear in a recipe-defining expression (see *Alchemization*).

### Literals

#### Integers

Decimal digits, optionally prefixed with `-`. Used for array counts, alignments, and `MEASURE` values:

```
8
0
-1
```

Array counts and alignments must be non-negative, and alignments must be powers of two. Negative integers are therefore never usable as layout inputs; they are legal only as `MEASURE` values, which are general integer constants (a measure may be exported and read from !~ATH as a plain integer -- see *Modules and !~ATH Integration*).

#### Strings

Double-quoted, with the same escape sequences as !~ATH (`\\`, `\"`, `\n`, `\t`). Used only for captchalogue-code assertions in `PUNCHED`:

```
"Dm9?k1bX"
```

There are no float, boolean, array, or map literals in !^CAKE -- it describes layouts, it does not compute values.

---

## Recipes

A recipe is the unit of schema. It is one of:

1. A struct recipe: an ordered list of named ingredients, laid out in memory like a C `struct`.
2. A union recipe: a tagged union ("a marble cake") produced by the `||` operator.
3. An alchemized recipe: the result of `&&` or `||` applied to other recipes, bound to a name.

### Struct declaration

```
RECIPE Point {
    INGREDIENT x: SPOON;
    INGREDIENT y: SPOON;
}
```

A struct recipe is declared with `RECIPE`, a name, and a brace-delimited body of `INGREDIENT` declarations and optional layout directives. The trailing `;` after the closing `}` is optional (the block self-terminates, as a `RITE` does in !~ATH).

Each ingredient has a name and a type:

```
INGREDIENT <name>: <ingredient-type>;
```

Ingredient names must be unique within a recipe. Ingredients are laid out in declaration order; order is significant and is part of the recipe's identity (see *Captchalogue Codes*).

An empty recipe is legal:

```
RECIPE Generic {}
```

The empty recipe is the perfectly generic object. It has size 0, alignment 1, and the reserved captchalogue code `00000000` (see *Reserved code*).

### Named alchemized declaration

```
RECIPE Sprite = Salamander && Cake;
RECIPE Shape  = Circle || Square || Triangle;
```

The right-hand side is an alchemy expression (see *Alchemization*). The result is a new recipe bound to the name. The trailing `;` is required for this form.

---

## Ingredient Types

An ingredient's type is one of: a scalar, a nested recipe (by value), a crust (a pointer to a recipe), or an array of any of these.

### Scalar ingredients

There are two families of scalar. Fixed-width scalars have the same size and byte order on every platform and are what you want for anything that crosses a wire. Native scalars mirror the !~ATH FFI marshalling table (so a recipe field and an FFI parameter largely speak the same language), at the cost of platform-dependent width. The one deliberate divergence is `BOOLEAN`: a recipe stores it as a 1-byte `_Bool` (the width a C `struct` actually uses for a `bool` member), whereas a scalar FFI `BOOLEAN` *parameter* widens to `int` -- the same keyword in two contexts.

#### Fixed-width scalars

Integers are unsigned by default (an ingredient is an amount, and you cannot have negative flour!) Prefix with `SIGNED` for a two's-complement signed integer. Widths are named for baking measures, ascending:

| keyword         | C type (unsigned / `SIGNED`)   | bytes | alignment |
|-----------------|--------------------------------|-------|-----------|
| `PINCH`         | `uint8_t`  / `int8_t`          | 1     | 1         |
| `DASH`          | `uint16_t` / `int16_t`         | 2     | 2         |
| `SPOON`         | `uint32_t` / `int32_t`         | 4     | 4         |
| `CUP`           | `uint64_t` / `int64_t`         | 8     | 8         |
| `DROP`          | `float`  (IEEE-754 binary32)   | 4     | 4         |
| `DOLLOP`        | `double` (IEEE-754 binary64)   | 8     | 8         |

`SIGNED` is only valid on the integer measures (`PINCH`, `DASH`, `SPOON`, `CUP`). `SIGNED DROP` and `SIGNED DOLLOP` are a compile error (floats are always signed).

The alignments in this table are **mandated by !^CAKE**, not inherited from the host C ABI. In particular `CUP` and `DOLLOP` are 8-aligned on every target -- including i386, where C aligns an 8-byte scalar to 4. This is precisely what lets a fixed-width recipe produce the same layout (and therefore the same captchalogue code) everywhere. The trade-off is that a non-`DENSE` fixed-width recipe may not match a native i386 `struct`; when host-ABI match is what you need, use native scalars.

```
INGREDIENT age:      PINCH;          // uint8_t
INGREDIENT delta:    SIGNED SPOON;   // int32_t
INGREDIENT velocity: DROP;           // float
```

#### Native scalars

These mirror the FFI table (with the `BOOLEAN` caveat noted above). Their size and/or byte order are **platform-dependent** and so they belong in recipes intended for same-machine FFI, not in a portable wire format:

| keyword   | C type        | notes                                                         |
|-----------|---------------|---------------------------------------------------------------|
| `INTEGER` | `long`        | 8 bytes on LP64; 4 bytes on LLP64 (Windows) and LP32 (wasm).  |
| `BOOLEAN` | `_Bool`       | 1 byte, value 0 or 1.                                         |
| `STRING`  | `const char*` | A pointer. Pointer-sized. Not owned by the buffer.            |
| `RELIC`   | `void*`       | A pointer. Pointer-sized. The opaque-pointer type from !~ATH. |

`STRING` in a recipe is a *pointer*, exactly as in the FFI table -- the bytes live elsewhere and the recipe stores only the address. A recipe is a fixed layout; it cannot embed a variable-length string inline. For inline text, use an array of `PINCH`.

`VOID` is **not** an ingredient type. A recipe always describes storage; no *scalar* ingredient has zero size. (A zero-size *ingredient* is still possible -- via a zero-count array or an embedded empty recipe -- so the empty recipe is not the only thing in the language with size 0.)

### Nested recipes (by value)

An ingredient may have another recipe as its type, embedding that recipe's layout inline:

```
RECIPE Segment {
    INGREDIENT start: Point;     // embeds Point's 8 bytes here
    INGREDIENT end:   Point;
}
```

The nested recipe must already be defined (or imported). Embedding contributes the nested recipe's size and alignment. A recipe that embeds itself by value, directly or through a cycle of by-value embeddings, is a COLLAPSED SOUFFLÉ error (it would have infinite size). Break the cycle with a crust.

### Crusts (pointers to recipes)

`CRUST OF <Recipe>` is a pointer to a recipe -- a `struct Recipe*`. A crust holds its filling at arm's length: it contributes only pointer size and alignment, never the pointee's size, so it is how you build recursive and linked layouts:

```
RECIPE Node {
    INGREDIENT value: SPOON;
    INGREDIENT next:  CRUST OF Node;   // struct Node* -- legal; no soufflé collapse
}
```

A crust is a pointer; what it points at is not managed by !^CAKE or by the baked buffer. Dereferencing is the caller's responsibility, on the !~ATH or C side.

### Arrays

`<count> OF <type>` is a fixed-length array. The count is a non-negative integer literal (or a `MEASURE` reference); the element type is any scalar, nested recipe, or crust:

```
INGREDIENT name:  16 OF PINCH;        // char name[16]  -- inline text
INGREDIENT verts: 3 OF Point;         // Point verts[3]
INGREDIENT grid:  4 OF (4 OF SPOON);  // uint32_t grid[4][4]
```

An array has the alignment of its element and a size of `count × stride`, where `stride` is the element size rounded up to the element alignment (i.e., `sizeof` of the element). A zero-count array has size 0 but retains its element's alignment.

### Reserved ingredients (padding)

An ingredient named `_` is reserved: it occupies space and participates in layout exactly like any other ingredient, but it is inaccessible to `SCOOP`/`SPRINKLE` and is excluded from no part of the captchalogue code (its name `_` is hashed like any other). Use it for explicit padding or wire-format reserved fields:

```
RECIPE Header {
    INGREDIENT magic:   SPOON;
    INGREDIENT _:       3 OF PINCH;   // 3 reserved bytes
    INGREDIENT version: PINCH;
}
```

Multiple `_` ingredients may appear in one recipe (the uniqueness rule does not apply to `_`).

---

## Layout and Alignment

A recipe lays out exactly like a C aggregate under the platform ABI.

### Algorithm (struct recipes)

Let the recipe's running offset start at 0 and its alignment start at 1.

1. For each ingredient in declaration order, with size `s` and alignment `a`:
   - Round the running offset *up* to the nearest multiple of `a`. The bytes skipped are padding.
   - Place the ingredient at that offset. Advance the running offset by `s`.
   - Set the recipe alignment to `max(recipe alignment, a)`.
2. After all ingredients, apply `RISE TO n` if present: recipe alignment becomes `max(recipe alignment, n)`.
3. The recipe's size is the final running offset rounded *up* to the recipe alignment. The bytes added by this final rounding are frosting (tail padding).

This is the standard rule used by the System V and Windows x64 ABIs; a recipe of native C types matches the C compiler's `struct` byte-for-byte.

### Directives and modifiers

- **`DENSE`** (modifier before `RECIPE`): a packed recipe. Every ingredient alignment is forced to 1, so no padding is ever inserted and the recipe alignment is 1 (unless raised by `RISE TO`). Equivalent to `#pragma pack(1)`.

  ```
  DENSE RECIPE Wire {
      INGREDIENT tag:  PINCH;
      INGREDIENT data: CUP;     // sits at offset 1, unaligned -- no padding
  }
  ```

- **`IMPERIAL`** (modifier before `RECIPE`): the Empire's byte order. Every multi-byte fixed-width scalar is stored **big-endian** (network order). The default is host byte order. Because endianness is meaningful only for portable layouts, an `IMPERIAL` recipe may contain **only** fixed-width scalars, nested `IMPERIAL` recipes, and arrays thereof -- no native scalars and no crusts (their byte order is not portable). Violating this is a compile error.

- **`RISE TO <n>;`** (statement inside a recipe body): raise the recipe's alignment to at least `n`, where `n` is a power of two. This can only increase alignment (and therefore tail frosting), never decrease it. It applies to the recipe as a whole regardless of where it appears in the body.

`DENSE` and `IMPERIAL` may be combined (`DENSE IMPERIAL RECIPE ...`); order does not matter.

### Native widths by platform

Fixed-width scalars are identical everywhere (their alignments are mandated by !^CAKE, not derived from the host ABI; see *Fixed-width scalars*). Only some native scalars vary. For example:

| platform                 | `INTEGER` (`long`) | pointer (`STRING`/`RELIC`/crust) |
|--------------------------|--------------------|----------------------------------|
| LP64 (Linux/macOS x86-64)| 8 bytes, align 8   | 8 bytes, align 8                 |
| LLP64 (Windows x64)      | 4 bytes, align 4   | 8 bytes, align 8                 |
| LP32 (wasm32-wasi)       | 4 bytes, align 4   | 4 bytes, align 4                 |
| ILP32 (i686 Linux)       | 4 bytes, align 4   | 4 bytes, align 4                 |

A recipe that uses `INTEGER`, a pointer-typed native scalar (`STRING` or `RELIC`), or a crust therefore has a platform-dependent layout -- and, by design, a platform-dependent captchalogue code (sizes feed the hash; see below). Two machines with different `long` or pointer widths compute different codes for the same source, so their handshake fails loudly (`STALE`) instead of silently corrupting data. A recipe whose only native scalar is `BOOLEAN` (1 byte everywhere) is *not* affected, and a recipe built entirely from fixed-width scalars has the same code on every platform -- because !^CAKE mandates those types' alignments rather than deferring to the host ABI.

---

## Alchemization

Recipes compose through the two alchemization operators from Homestuck's alchemy system. Both take recipes and produce a recipe; both may only appear on the right-hand side of a `RECIPE Name = ...;` declaration or nested in parentheses.

### `&&` -- combine (merge)

`A && B` produces a struct recipe containing all of A's ingredients, in order, followed by each of B's ingredients that A does not already have. "Already have" means same name:

- Same name and identical resolved type: the ingredients are the same; B's is dropped (A's position and type win). The field is not duplicated.
- Same name, different type: a CURDLED error. The cake has split; you cannot merge `x: SPOON` with `x: DOLLOP`.

`&&` is associative but **not** commutative -- `A && B` orders A's fields first, `B && A` orders B's first, and the two generally have different layouts and different codes. (Alchemy has always cared about which item you `&&` onto which.)

```
RECIPE Named    { INGREDIENT id: SPOON; INGREDIENT name: 16 OF PINCH; }
RECIPE Located  { INGREDIENT id: SPOON; INGREDIENT pos:  Point; }

RECIPE Entity = Named && Located;
// id (shared, deduped), name, pos
```

### `||` -- marble cake (tagged union)

`A || B` produces a union recipe: a one-byte flavor tag followed by a payload large enough for either arm. Reading it requires first asking which flavor you are looking at -- pleasingly inconvenient, and faithful to a fetch modus.

Layout of a union of arms `A_0 .. A_{k-1}`:

- An implicit `FLAVOR` tag occupies offset 0. It is a `PINCH` (`uint8_t`), so a union may have at most 256 arms (more is a compile error).
- The payload begins at `align_up(1, P)`, where `P = max` alignment over all arms.
- The payload size is `max` size over all arms. Each arm's own struct layout sits at the payload offset; an arm field `f` of arm `Arm` lives at `payloadOffset + offsetof(Arm, f)`.
- The union's alignment is `max(P, RISE TO n if any)` (and `1` under `DENSE`, which forces `P = 1`). Its size is `align_up(payloadOffset + payloadSize, alignment)`.

Flavor assignment is by sorted code, which makes `||` commutative. The arms are sorted by their captchalogue code (lexicographically, under the code alphabet's ordering); the arm with the smallest code is `FLAVOR` 0, the next is 1, and so on. Therefore `A || B` and `B || A` are the same union, with the same layout and the same code. Two arms that are the same recipe (equal codes) collapse to a single flavor.

Each arm is named by the recipe that produced it; that name is how you path into it from !~ATH (`SCOOP(buf, U, "Circle.radius")`). Arms in a chained `A || B || C` are flattened: it is a single 3-arm union, not a union containing a union.

```
RECIPE Circle { INGREDIENT r: SPOON; }
RECIPE Box    { INGREDIENT w: SPOON; INGREDIENT h: SPOON; }

RECIPE Shape = Circle || Box;
// FLAVOR: PINCH @ 0
// payload @ 4 (max arm align = 4): either Circle{r} or Box{w,h}
// size 12
```

### Precedence

`&&` binds tighter than `||`. Parenthesize to override. Mixing without parentheses is legal but discouraged in schemas meant to be read by humans:

```
RECIPE R = A && B || C;     // == (A && B) || C  (&& binds tighter)
RECIPE S = A && (B || C);   // compile error: a union may not be an && operand
```

`&&` merges *struct* recipes only. **Both operands must be struct recipes; using a union (a `||` result) as an `&&` operand is a compile error.** To put a marble cake inside a struct, embed it as an ordinary ingredient of a named union type (`INGREDIENT shape: Shape;`), whose `rec(<code>)` descriptor already covers unions -- there is no `&&` form for it, and none is needed.

---

## Captchalogue Codes

Every recipe reduces to an 8-character captchalogue code. The code is a hash of the recipe's fully resolved canonical form. It is an **integrity check, not a security boundary**: 48 bits is plenty to catch an accidental schema drift between two peers and nowhere near enough to resist a deliberate collision. Do not use it as one.

### Alphabet

The code uses Homestuck's 64-character captchalogue code alphabet. The character at index `i` (0-63) is:

| index range | characters          |
|-------------|---------------------|
| 0-9         | `0` `1` ... `9`     |
| 10-35       | `A` `B` ... `Z`     |
| 36-61       | `a` `b` ... `z`     |
| 62          | `?`                 |
| 63          | `!`                 |

Eight characters at 6 bits each encode a 48-bit hash. Code strings are compared (for union sorting and for handshakes) by index order under this alphabet.

### Canonical form

The code hashes a canonical descriptor string: a deterministic, single-line UTF-8 serialization of the recipe's resolved layout. The descriptor includes everything that affects meaning or layout and excludes everything that does not. In particular it includes ingredient names (the hash is nominal: two recipes with identical layout but different field names get different codes, because "same bytes, different meaning" is exactly the bug a code should catch) and excludes the recipe's own binding name (renaming the type alias does not change the wire format).

A struct recipe serializes as:

```
S(e=<H|B>,p=<0|1>,a=<align>,z=<size>){<field>;<field>;...}
```

where `e` is endianness (`H` host / `B` big-endian under `IMPERIAL`), `p` is 1 under `DENSE` else 0, `a` is alignment, `z` is size. Each field serializes as:

```
<name>:<typedesc>@<offset>+<fieldsize>
```

A union recipe serializes as:

```
U(e=...,p=...,a=...,z=...){<flavor>;<flavor>;...}
```

with flavors in `FLAVOR`-index order (i.e., sorted by arm code), each as:

```
<armName>=<armCode>@<payloadOffset>
```

The `<typedesc>` for each ingredient type:

| ingredient                | typedesc                                    |
|---------------------------|---------------------------------------------|
| `PINCH` / `SIGNED PINCH`  | `u8` / `i8`                                 |
| `DASH`  / `SIGNED DASH`   | `u16` / `i16`                               |
| `SPOON` / `SIGNED SPOON`  | `u32` / `i32`                               |
| `CUP`   / `SIGNED CUP`    | `u64` / `i64`                               |
| `DROP`                    | `f32`                                       |
| `DOLLOP`                  | `f64`                                       |
| `BOOLEAN`                 | `bool`                                      |
| `INTEGER`                 | `long` (its `+<fieldsize>` carries width)   |
| `STRING` / `RELIC`        | `ptr` (its `+<fieldsize>` carries width)    |
| `CRUST OF Name`           | `ptr` (target not encoded; see below)       |
| nested recipe `R`         | `rec(<R's 8-char code>)`                     |
| array `n OF T`            | `[<n>]<T's typedesc>`                        |

Two consequences worth stating:

- A nested by-value recipe contributes its own captchalogue code, so hashing is compositional and changing a nested recipe ripples outward into every code that embeds it.
- A crust contributes only `ptr`; the pointee is **not** encoded at all -- neither its code nor its name. A pointer's representation does not depend on what it points at, so this matches reality, keeps the code independent of the pointee's name (renaming a recipe never changes the code of anything that merely points at it, mirroring the rule for a recipe's own binding name), and breaks at the pointer the hash cycle that a recursive recipe (`Node` with `CRUST OF Node`) would otherwise create. The cost is that two crusts of different recipes are indistinguishable in the hash; since a crust crosses neither a wire nor an ABI boundary as anything but an address, this is intended.

Because `<offset>`, `+<fieldsize>`, `a`, and `z` are all in the descriptor, *any* layout difference -- a platform's wider `long`, an inserted `RISE TO`, a `DENSE` modifier -- changes the code.

### Hash function

The code is computed from the canonical descriptor's UTF-8 bytes by FNV-1a (64-bit), folded to 48 bits, then base-64 encoded MSB-first. Every implementation must reproduce this exactly:

```
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME  = 0x00000100000001B3

h = FNV_OFFSET
for each byte b in utf8(canonical_descriptor):
    h = h XOR b
    h = (h * FNV_PRIME) mod 2^64

h48 = (h XOR (h >> 48)) AND 0xFFFFFFFFFFFF      // fold top 16 bits down, keep low 48

code = ""
for i in 0..7:
    sextet = (h48 >> (6 * (7 - i))) AND 0x3F     // most-significant sextet first
    code = code + ALPHABET[sextet]
```

### Reserved code

The empty recipe (zero ingredients, the *perfectly generic object*) has the code `00000000` by definition; its descriptor is not hashed. No non-empty recipe is given `00000000` even if its hash happened to land there -- in the astronomically unlikely event that the fold produces all-zero sextets for a non-empty recipe, an implementation must perturb it (e.g., re-encode `h48 | 1`). `00000000` means "generic" and nothing else.

### `PUNCHED` assertions

A recipe may assert its own code in source. The compiler computes the code and raises **STALE** if it disagrees:

```
RECIPE Point PUNCHED "Dm9?k1bX" {
    INGREDIENT x: SPOON;
    INGREDIENT y: SPOON;
}
```

This turns a code into a schema lockfile embedded in the source: you cannot change the wire format without the assertion screaming at you in the diff, and tooling can auto-insert or refresh the string. `PUNCHED` is valid on any recipe declaration form (struct or named alchemy). The code strings shown in this document's examples are illustrative -- treat them as placeholders, not as values you can verify by hand.

---

## Measures

A `MEASURE` is a named integer constant, for reuse in array counts and alignments:

```
MEASURE NAME_LEN = 16;
MEASURE WORD = 4;

RECIPE Record {
    INGREDIENT name: NAME_LEN OF PINCH;
    RISE TO WORD;
}
```

Measures are the only non-recipe declaration. They are pure integers, evaluated at parse time; there is no expression language (no arithmetic, no other operators). A measure may be exported and read from !~ATH as a plain integer (see below).

---

## Modules and !~ATH Integration

### Importing a `.^CAKE` file

A `.^CAKE` file is loaded by !~ATH through the existing watcher-module mechanism, exactly parallel to importing a `.~ATH` module -- but a `.^CAKE` module is declarative, so it has no `THIS` and needs no `THIS.DIE()`:

```
import watcher Shapes("./shapes.^CAKE");

UTTER(CAPTCHA(Shapes.Point));     // e.g. "Dm9?k1bX"
UTTER(SIZEOF(Shapes.Point));      // 8
```

- Every top-level `RECIPE` and `MEASURE` becomes an export accessible as `Module.Name`. Recipes export as recipe values; measures export as integers.
- `TYPEOF(Shapes.Point)` is `"RECIPE"`.
- The watcher entity behaves normally: it dies when the file is deleted. Re-importing re-parses and refreshes exports.
- A `.^CAKE` file that fails to parse, that contains a `CURDLED`/`COLLAPSED SOUFFLÉ`/`STALE` error, or that an `IMPERIAL` portability rule rejects, raises a catchable runtime error at import time.

### Recipe values and built-in rites

Once imported, a recipe is a first-class !~ATH value (like a rite reference: passable and comparable, but inert on its own). !^CAKE support adds these built-in rites to !~ATH. The only mutable value type in !~ATH is `BUFFER`, so a baked recipe instance *is* a `BUFFER` -- a real, exactly-sized C struct with no header, ready to hand to the FFI.

| rite                                  | result | description                                                            |
|---------------------------------------|--------|------------------------------------------------------------------------|
| `CAPTCHA(recipe)`                     | STRING | the 8-character captchalogue code.                                     |
| `SIZEOF(recipe)`                      | INTEGER| byte size of a baked instance.                                        |
| `BAKE(recipe)`                        | BUFFER | a fresh zeroed buffer of `SIZEOF(recipe)` bytes.                       |
| `SPRINKLE(buf, recipe, path, value)` | VOID   | write a field (see *Field value types*); mutates `buf` in place.       |
| `SCOOP(buf, recipe, path)`           | value  | read a field (see *Field value types* for the returned type).         |
| `FLAVOR(buf, recipe)`                | INTEGER| active flavor index of a union buffer (reads the tag).                |
| `PLATE(buf, recipe)`                 | BUFFER | a wire envelope: the 8 code bytes followed by `buf`'s bytes.          |
| `TASTE(plated)`                      | STRING | the leading 8 bytes of a plated buffer as a code string (cheap peek). |
| `UNPLATE(plated, recipe)`            | BUFFER | verify the leading code matches; return the struct bytes (code stripped).|

**Paths:** `path` is a string naming a field, dotted for nesting and numeric-indexed for arrays: `"x"`, `"origin.x"`, `"verts.2"`, `"verts.2.y"`. For a union buffer, the first path segment is the arm name: `"Circle.r"`. The implicit union tag is the path `"FLAVOR"`; `SPRINKLE(buf, U, "FLAVOR", 1)` sets the active flavor, and `SCOOP(buf, U, "FLAVOR")` reads it (equivalently `FLAVOR(buf, U)`).

**Field value types:** an integer field (`PINCH`..`CUP`, `INTEGER`, `BOOLEAN`) reads and writes as an !~ATH `INTEGER` (`BOOLEAN` as `0`/`1`); `DROP`/`DOLLOP` as a `FLOAT`. A pointer-typed field (`STRING`, `RELIC`, or a crust) reads as a **loose `RELIC`** -- the raw stored address, with no owning session -- and writes from a `RELIC`; `SCOOP` does **not** dereference it (the buffer does not own what the pointer points at, so chasing it is the caller's responsibility). A nested-recipe field reads as a fresh `BUFFER` copy of that sub-region and writes by `memcpy` from a `BUFFER` of the matching size (`RAW` on a size mismatch). The reserved ingredient `_` is inaccessible to both `SCOOP` and `SPRINKLE` (`RAW`).

**Endianness:** `SPRINKLE`/`SCOOP` honor the recipe's byte order: writing to an `IMPERIAL` field stores big-endian and reading reconstructs the host integer, transparently.

**The handshake:** Before exchanging data, two peers compare codes:

```
import watcher Wire("./wire.^CAKE");

// sender:
BIRTH msg WITH BAKE(Wire.Packet);
SPRINKLE(msg, Wire.Packet, "seq", 7);
BIRTH framed WITH PLATE(msg, Wire.Packet);     // 8 code bytes + struct
// ... send `framed` over a connection ...

// receiver, having read `framed` into a buffer:
ATTEMPT {
    BIRTH body WITH UNPLATE(framed, Wire.Packet);   // STALE if codes differ
    BIRTH seq  WITH SCOOP(body, Wire.Packet, "seq");
    UTTER("seq:", seq);
} SALVAGE err {
    UTTER("schema mismatch:", err);                  // schema mismatch: STALE ...
}
```

### Foreign sessions: structs by value

This is the gap !^CAKE was built to close. A `TRANSCRIBE` may name a recipe (in scope, possibly dotted through a module) as a parameter or return type. The runtime builds the `ffi_type` element array from the recipe descriptor, so the struct passes by value with no wrapper shim:

```
import watcher Geo("./geo.^CAKE");

import session Lib("./libgeo.so") {
    // C: struct Point translate(struct Point p, long dx, long dy);
    TRANSCRIBE translate(Geo.Point, INTEGER, INTEGER) -> Geo.Point;
}

BIRTH p WITH BAKE(Geo.Point);
SPRINKLE(p, Geo.Point, "x", 3);
SPRINKLE(p, Geo.Point, "y", 4);

BIRTH moved WITH Lib.translate(p, 10, 10);     // moved is a BUFFER (Geo.Point)
UTTER(SCOOP(moved, Geo.Point, "x"));           // 13
```

Marshalling rules for recipe-typed transcriptions:

- A recipe parameter expects a `BUFFER` baked from that exact recipe. A buffer of the wrong size is a `RAW` error (catchable).
- A recipe return yields a fresh `BUFFER` of `SIZEOF(recipe)`.
- The recipe must use native scalar widths (or fixed-width scalars whose layout the host C ABI also produces) for the struct to match the C side; an `IMPERIAL` recipe is not a valid FFI type (its byte order is not the host's). Recipe-typed parameters count toward the FFI's per-transcription parameter limit as a single parameter.

---

## Errors

!^CAKE errors are themed. Compile-time errors are raised when the `.^CAKE` file is parsed (at !~ATH import time, where they surface as catchable runtime errors); the rest are raised by the built-in rites and are catchable with `ATTEMPT`/`SALVAGE` on the !~ATH side.

| error               | when                                                                                       |
|---------------------|--------------------------------------------------------------------------------------------|
| `CURDLED`           | `&&` merge conflict: same field name, incompatible type. (Compile-time.)                   |
| `COLLAPSED SOUFFLÉ` | a recipe embeds itself by value, directly or through a by-value cycle. Use a crust. (Compile-time.) |
| `STALE`             | a captchalogue code mismatch: a failed `PUNCHED` assertion (compile-time), or `UNPLATE`/handshake code disagreement (runtime). |
| `RAW`               | something undercooked at runtime: buffer too short for the recipe, an unknown or ill-typed `path`, a value that does not fit the field, a recipe-typed FFI argument of the wrong size, or `FLAVOR` on a non-union. |
| `OVERBAKED`         | a buffer is longer than the recipe expects: trailing bytes on `UNPLATE` / strict decode. (Runtime.) |

---

## Grammar (EBNF)

```ebnf
file            = { declaration } ;

declaration     = recipe_decl | measure_decl ;

measure_decl    = "MEASURE" IDENTIFIER "=" INTEGER ";" ;

recipe_decl     = struct_recipe | alchemy_recipe ;

struct_recipe   = { recipe_modifier } "RECIPE" IDENTIFIER [ punch ]
                  "{" { recipe_member } "}" [ ";" ] ;

alchemy_recipe  = "RECIPE" IDENTIFIER [ punch ] "=" alchemy_expr ";" ;

recipe_modifier = "DENSE" | "IMPERIAL" ;

punch           = "PUNCHED" STRING ;

recipe_member   = ingredient_decl | rise_directive ;

ingredient_decl = "INGREDIENT" ingredient_name ":" ingredient_type ";" ;
ingredient_name = IDENTIFIER | "_" ;

rise_directive  = "RISE" "TO" int_value ";" ;

ingredient_type = array_type | element_type ;
array_type      = int_value "OF" ( element_type | "(" array_type ")" ) ;
element_type    = scalar_type | crust_type | IDENTIFIER ;   // IDENTIFIER = nested recipe

crust_type      = "CRUST" "OF" IDENTIFIER ;

scalar_type     = fixed_int | fixed_float | native_scalar ;
fixed_int       = [ "SIGNED" ] ( "PINCH" | "DASH" | "SPOON" | "CUP" ) ;
fixed_float     = "DROP" | "DOLLOP" ;
native_scalar   = "INTEGER" | "BOOLEAN" | "STRING" | "RELIC" ;

// && binds tighter than || ; both are left-associative.
alchemy_expr    = union_expr ;
union_expr      = merge_expr { "||" merge_expr } ;
merge_expr      = alchemy_atom { "&&" alchemy_atom } ;
alchemy_atom    = IDENTIFIER | "(" alchemy_expr ")" ;

int_value       = INTEGER | IDENTIFIER ;    // IDENTIFIER = a MEASURE reference
```

### Semantic notes on the grammar

The grammar is permissive; the following are enforced semantically:

1. Alignment values (`RISE TO`, and any `MEASURE` used as one) must be positive powers of two.
2. Array counts must be non-negative integers (a `MEASURE` used as a count must be non-negative).
3. `SIGNED` is valid only on `PINCH`/`DASH`/`SPOON`/`CUP`, never on floats or native scalars.
4. `IMPERIAL` recipes may contain only fixed-width scalars, `IMPERIAL` nested recipes, and arrays of those -- no native scalars, no crusts.
5. Ingredient names are unique within a recipe, except `_`, which may repeat.
6. A nested-recipe `IDENTIFIER` and a crust's `IDENTIFIER` must resolve to a recipe already defined in the file or imported; a by-value cycle is `COLLAPSED SOUFFLÉ`.
7. Reserved keywords (the scalar type names, `CRUST`, `FLAVOR`, etc.) cannot be used as recipe, ingredient, or measure names.
8. A union (`||` result) has at most 256 arms.
9. Both operands of `&&` must be struct recipes; a union (`||` result) as an `&&` operand is a compile error.

---

## Example Schemas

### A point and a segment

```
// geo.^CAKE
RECIPE Point {
    INGREDIENT x: SIGNED SPOON;
    INGREDIENT y: SIGNED SPOON;
}

RECIPE Segment {
    INGREDIENT start: Point;
    INGREDIENT end:   Point;
}
```

`Point` is 8 bytes, alignment 4. `Segment` embeds two by value: 16 bytes, alignment 4.

### A packed, big-endian wire header

```
// wire.^CAKE
DENSE IMPERIAL RECIPE Packet PUNCHED "9bX!a02k" {
    INGREDIENT magic:   SPOON;        // big-endian uint32, offset 0
    INGREDIENT version: PINCH;        // offset 4 (packed: no padding)
    INGREDIENT _:       PINCH;        // 1 reserved byte
    INGREDIENT seq:     DASH;         // big-endian uint16, offset 6
}
// size 8, alignment 1
```

### A linked list (recursion through a crust)

```
// list.^CAKE
RECIPE Node {
    INGREDIENT value: SIGNED CUP;
    INGREDIENT next:  CRUST OF Node;   // breaks the soufflé; pointer-sized
}
```

### A marble cake (tagged union)

```
// shape.^CAKE
RECIPE Circle { INGREDIENT r: SPOON; }
RECIPE Rect   { INGREDIENT w: SPOON; INGREDIENT h: SPOON; }

RECIPE Shape = Circle || Rect;
```

From !~ATH:

```
import watcher Sh("./shape.^CAKE");

BIRTH s WITH BAKE(Sh.Shape);
SPRINKLE(s, Sh.Shape, "FLAVOR", FLAVOR_OF_RECT);   // set the active arm's flavor index
SPRINKLE(s, Sh.Shape, "Rect.w", 30);
SPRINKLE(s, Sh.Shape, "Rect.h", 20);

SHOULD FLAVOR(s, Sh.Shape) == FLAVOR_OF_RECT {
    UTTER("area:", SCOOP(s, Sh.Shape, "Rect.w") * SCOOP(s, Sh.Shape, "Rect.h"));
}

THIS.DIE();
```

(`FLAVOR_OF_RECT` stands in for the integer flavor index, which is fixed by the arms' sorted codes; in practice you read it once with `FLAVOR` after building, or agree on it out of band.)

### Merging two recipes

```
// entity.^CAKE
RECIPE Named   { INGREDIENT id: SPOON; INGREDIENT name: 16 OF PINCH; }
RECIPE Placed  { INGREDIENT id: SPOON; INGREDIENT at:   2 OF SIGNED SPOON; }

RECIPE Entity = Named && Placed;
// id (shared), name[16], at[2] -- a single 28-byte struct, alignment 4
```

---

## Implementation Notes

!^CAKE is not yet implemented. When it is, the intended shape is:

- A `.^CAKE` parser and layout/hash engine, callable by the !~ATH transpiler at watcher-module import time, exposing recipes as values and adding the built-in rites (`BAKE`, `SCOOP`, `SPRINKLE`, `CAPTCHA`, `SIZEOF`, `FLAVOR`, `PLATE`, `TASTE`, `UNPLATE`) to the runtime.
- An extension to the foreign-session machinery that turns a recipe descriptor into an `ffi_type` element array, enabling by-value struct parameters and returns in `TRANSCRIBE`.
- Optionally a standalone tool that emits a C header (the `struct` definitions) and a manifest of captchalogue codes from a `.^CAKE` file, so a C peer can share the exact layout and participate in the code handshake.

### Current limitations (anticipated)

These are scoped out of the first version and may be added later:

- No bitfields. Sub-byte packing is not expressible; the smallest ingredient is one byte (`PINCH`).
- No schema evolution / optional fields. Any change to a recipe is a new cake with a new code. This is what keeps the hash model clean; versioning, when it comes, will be a marble cake of `v1 || v2`.
- No variable-length inline data. A recipe is a fixed layout. Variable-length text or arrays live behind a `STRING`/`RELIC`/crust pointer, not inline (use a fixed `n OF PINCH` for bounded text).
- No expression language. `MEASURE` values and array counts are integer literals or measure references; there is no arithmetic.
- Recipes using `INTEGER` or pointer types (`STRING`/`RELIC`/crust) are platform-specific by design (and so are their codes); use fixed-width scalars for anything portable.
- No mortal recipes. Recipes are not entities; you cannot `~ATH` a recipe. Schemas are eternal.

---

This project is licensed under the GNU General Public License v2.0. See the LICENSE file for details.
