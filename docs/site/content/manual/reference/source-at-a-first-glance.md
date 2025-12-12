---
title: "Source at a First Glance"
date: 2025-12-10
---

# Source at a First Glance

When you unzip Pixie-src-X.Y.Z, you should get the following directory structure:

- Pixie
  - doc - *The HTML documentation*
  - man - *Manual entries for various programs*
  - shaders - *Contains default shaders*
  - src - *The source code*
    - common - *A library for common functions*
    - dsotest - *An example DSO shadeop*
    - file - *"file" display driver*
    - framebuffer - *"framebuffer" display driver*
    - gui - *A dynamic library that provides user interface functionality*
    - openexr - *"openexr" display driver*
    - precomp - *A program that generates misc. code for Pixie*
    - rgbe - *"rgbe" display driver*
    - ri - *The main RenderMan Ri library*
    - rndr - *The program that uses "ri" to render your RIB files*
    - sdr - *A library for parsing compiled shaders*
    - sdrc - *The RenderMan Shading Language compiler*
    - sdrinfo - *A program that uses sdr to display information about compiled shaders*
    - show - *A program that displays various data computed using "ri"*
    - texmake - *A program that converts textures into Pixie's format*
  - textures - *Contains the default textures*
  - win32inst - *Inno setup files for creating an installer*
  - windows - *Windows Visual Studio projects*
    - vcnet8 - *For .NET 2005*
      - Pixie - *This directory contains the project files*