// Force-included into every portable C++ test (see tools/check.py).
// Mirrors the object-like macros from the pinned Union SDK
// (UnionProject/UnionSDK/utils/Macro.h, commit 4ae1fc51) that silently
// replace ordinary identifiers. A header that declares e.g. `Invalid`
// or `None` compiles with g++ but fails under MSVC with the real SDK;
// with this shim it fails here first.
#pragma once
#define True         (1)
#define False        (0)
#define Yes          True
#define No           False
#define Null         (nullptr)
#define None         (0)
#define Invalid      (-1)
#define HookDelayed  (0)
#define Undefined    Invalid
#define NotFound     Invalid
#define Success      True
#define Failed       False
