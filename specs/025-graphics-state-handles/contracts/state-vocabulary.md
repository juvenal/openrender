# Contract: State vocabulary header

**Owner**: Graphics State (`src/ri/state/stateVocabulary.h`).
**Requirements**: FR-008, FR-009, SC-007.

```cpp
enum class EDisplayParameterType : int { Float = 0, Vector = 1, Matrix = 2, String = 3, Integer = 4 };
enum class EBlobbyOrder : int { RiSpec = 0, AppNote = 1 };
```

## Rules

- **Scoped enums**, so no enumerator collides with `dsply.h`'s `FLOAT_PARAMETER…` or
  `blobbyField.h`'s `BLOBBY_ORDER_*` in a TU that includes both. Both of those headers stay
  byte-identical.
- **`COptions::CDisplay::TDisplayParameter::type`** is an `EDisplayParameterType`. The copy
  constructor's `switch` (`options.cpp:136-148`) uses the State names.
- **`COptions::blobbyOpcodeOrder`** stays `int` and defaults to `static_cast<int>(EBlobbyOrder::RiSpec)`.
  The core keeps assigning `BLOBBY_ORDER_*`, and Geometry keeps reading the `int`.
- **The renderer core converts where the vocabularies meet**, with one helper and a `static_assert`
  per pair beside it:
  - `rendererContext.cpp:1417-1448` writes `EDisplayParameterType::…`;
  - `rendererDisplay.cpp:48-57` compares the plug-in's `ParameterType` against the stored type.

  These are explicit conversions. C++20 forbids comparing a scoped enum with another type
  implicitly, and deprecates comparing two different unscoped enum types, which is an error under
  `-Werror`.
- **The display plug-in ABI does not change**, so there is no Semantic Versioning bump (DB-003).

## Consistency test

`State_VocabularyConsistency` (`tests/unit/state/`, a Consumer, label `state`) includes
`stateVocabulary.h`, `dsply.h` and `blobbyField.h`. It checks all seven pairs at run time and
names the first pair that differs. It is modelled on `Common_VocabularyConsistency`.

**Red evidence**:
- on `dfffba0` the header is missing, so the build fails;
- a scratch mutation of one value turns it red (`validation.md`).

**Limitation**: neither C enum has a count, so an enumerator *added* to `dsply.h` is not detected.
`dsply.h` is frozen in 025.
