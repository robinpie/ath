<!-- SPDX-License-Identifier: GPL-2.0-only -->
# !^CAKE

!^CAKE is a schema definition language inspired by ^CAKE, another of the fictional programming languages from Homestuck. !^CAKE is about baking: a `RECIPE` describes a C-compatible memory layout, recipes compose via alchemization operators (`&&` merge, `||` tagged union -- a marble cake), and every recipe deterministically compiles to an 8-character captchalogue code that both sides of a wire can compare before exchanging data.

The language specification is drafted in [`cakeSpec.md`](./cakeSpec.md).

The engine lives inside the !~ATH C runtime, not here.

A `.^CAKE` file is used from !~ATH by importing it as a watcher module:

```
import watcher Geo("./geo.^CAKE");
UTTER(CAPTCHA(Geo.Point));   // its 8-char captchalogue code
BIRTH p WITH BAKE(Geo.Point);
SPRINKLE(p, Geo.Point, "x", 3);
UTTER(SCOOP(p, Geo.Point, "x"));   // 3
```

See [`cakeSpec.md`](./cakeSpec.md) for details.