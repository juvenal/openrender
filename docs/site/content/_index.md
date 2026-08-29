---
title: "openRender Documentation"
date: 2025-12-08
weight: 1
---

# Welcome to openRender Documentation

The documentation here has been prepared on the online wiki [openRender Wiki](http://george-graphics.co.uk/openrenderwiki/). You can add to it by heading over there.

## Main Sections

- [Documentation](/openrender/manual/) - Documentation and reference on openRender's features
- [Tutorials](/openrender/manual/tutorials/) - Tutorial-style / How-To guides for openRender
- [FAQ](/openrender/development/faq/) - Frequently Asked Questions

You may also find these links useful:

- [openRender homepage](http://openrender.sourceforge.net/)
- [openRender on sourceforge](http://sourceforge.net/projects/openrender/)

## Documentation

How openRender relates to the RiSpec, and documentation on openRender's non-standard features and extensions.

- [Installing / running openRender](/openrender/manual/reference/installing-and-running/)
- [Multithreading](/openrender/manual/reference/multithreading/)
- [Hiders](/openrender/manual/reference/hiders/)
- [Display drivers](/openrender/manual/reference/display-drivers/)
- [Options](/openrender/manual/reference/options/)
- [Attributes](/openrender/manual/reference/attributes/)
- [Solid CSG Operations](/openrender/manual/reference/solid-csg-operations/)
- [Blobby Implicit Surfaces](/openrender/manual/reference/blobby-implicit-surfaces/)
- [Occlusion culling](/openrender/manual/reference/occlusion-culling/)
- [Baking 3D Textures](/openrender/manual/reference/baking-3d-textures/)
- [Network parallel rendering](/openrender/manual/reference/network-parallel-rendering/)
- [DSO shading](/openrender/manual/reference/dso-shading/)
- [Transparency shadow maps](/openrender/manual/reference/transparency-shadow-maps/)
- [Global illumination](/openrender/manual/reference/global-illumination/)
- [Point based occlusion and color bleeding](/openrender/manual/reference/point-based-gi/)
- [Raytracing in SL](/openrender/manual/reference/raytracing-in-sl/)
- [Raytraced shadows / reflections](/openrender/manual/reference/raytraced-shadows-and-reflections/)
- [Hardcoded shaders](/openrender/manual/reference/hardcoded-shaders/)
- [Shader library](/openrender/manual/reference/shader-library/)
- [Version management](/openrender/manual/reference/version-management/)
- [Performance / Quality Tips](/openrender/manual/reference/performance-and-quality-tips/)
- [Source at a Glance](/openrender/manual/reference/source-at-a-glance/)
- [Using openRender with Maya](/openrender/manual/reference/using-openrender-with-maya/)
- [Conditional RIB](/openrender/manual/reference/conditional-rib/)
- [RIB Resources](/openrender/manual/reference/rib-resources/)
- [Ptc API](/openrender/manual/reference/ptc-api/)
- [User Attributes And Options](/openrender/manual/reference/user-attributes-and-options/)
- [SL Functions](/openrender/manual/reference/sl-functions/)

## Examples / Tutorials

Tutorial-style guides to various features in openRender.

- [Basics, Running openRender](/openrender/manual/tutorials/basics-running-openrender/)
- [Raytraced shadows](/openrender/manual/tutorials/raytraced-shadows/)
- [Soft raytraced shadows](/openrender/manual/tutorials/soft-raytraced-shadows/)
- [Global Illumination](/openrender/manual/tutorials/global-illumination/)
- [Dispersion](/openrender/manual/tutorials/dispersion/)
- [Baking To Textures](/openrender/manual/tutorials/baketotexture/)

## News

**openRender 2.2.1 is out**! New features include:

- [Point Based occlusion and color bleeding](/openrender/manual/reference/point-based-gi/) - Get occlusion and color bleeding without raytracing
- Improved AOV support (color AOVs are alpha composited like rgba)
- Improved non-raster-orient dicing
- Raytracing improvements - more robust, faster raytracing, with PRMan-compatible visibility and shade attributes
- Shading language compiler `oshader` is more robust and supports some syntax it previously didn't

Download: [SourceForge Files](http://sourceforge.net/project/showfiles.php?group_id=59462&package_id=55537&release_id=522312), Release Notes: [SourceForge Notes](http://sourceforge.net/project/shownotes.php?release_id=522312&group_id=59462)