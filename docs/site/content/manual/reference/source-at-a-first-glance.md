---
title: "Source at a First Glance"
date: 2025-12-10
---

# Source at a First Glance

When you unzip openRender-src-X.Y.Z, you should get the following directory structure:

- openRender
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
    - precomp - *A program that generates misc. code for openRender*
    - rgbe - *"rgbe" display driver*
    - ri - *The main RenderMan Ri library*
    - orender - *The program that uses "ri" to render your RIB files*
    - rslo - *A library for parsing compiled shaders*
    - oshader - *The RenderMan Shading Language compiler*
    - rsloinfo - *A program that uses rslo to display information about compiled shaders*
    - oshow - *A program that displays various data computed using "ri"*
    - otexmake - *A program that converts textures into openRender's format*
  - textures - *Contains the default textures*
  - win32inst - *Inno setup files for creating an installer*
  - windows - *Windows Visual Studio projects*
    - vcnet8 - *For .NET 2005*
      - openRender - *This directory contains the project files*