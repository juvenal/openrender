# RIB Output Guide

## Overview

openRender provides a standardized RIB output mechanism through the `CRibOut` class. Recent updates have unified the initialization logic and added automatic emission of standard RenderMan RIB headers.

## Standard RIB Headers

When a RIB output is initialized (either via a filename or an existing file stream), the renderer now automatically emits a standard preamble to ensure compliance with the RenderMan RIB-Structure 1.1 specification.

### Example Header

```rib
##RenderMan RIB-Structure 1.1
##Creator openRender 1.0.0
##CreationDate Wed May 20 19:53:31 2026
```

- **RIB-Structure**: Declares compliance with the RIB structure 1.1.
- **Creator**: Identifies the renderer and its version.
- **CreationDate**: Records the timestamp when the RIB was generated.

## Implementation Details

### Unified Initialization

The `CRibOut` constructors have been refactored to use a shared `completeInit()` method. This ensures that the RIB preamble is consistently emitted regardless of whether the output is a file or a pipe/stdout.

### Language Bindings Parity

The **Python** and **Lua** bindings have also been updated to match this behavior. When the `Begin()` method is called in these bindings, the same standard header is written to the output stream, ensuring that RIB files generated from scripts are identical in structure to those generated from C++ or the core renderer.

## Usage

No changes are required from the user side. The headers are emitted automatically upon calling `RiBegin` or its equivalent in the language bindings.
