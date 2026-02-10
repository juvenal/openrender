# Developer Notes

## Open Issues

- Purging tessellations for raytracing
- Moving raytraced surface
- Efficient subdivision surface creases
- Subdivision highly creased surface issues
- ~~Bug: Terminating the framebuffer during rendering~~ (FIXED)
- Irradiance accuracy issues

## Development Notes

### Possible Optimization

(To be documented.)

### Todos

- OpenEXR input for textures
- RiDisplayChannel & support
- Additional attributes, options visible from SL
- ~~bake, pointcloud and brickmap support~~ (done)
- RiFilter support
- Trace subsets
- Patch crack stitching
- 64-bit compatibility  
  The main issue is the shaders keeping all the data in a linear array. This can be fixed by separating the opcodes from arguments.
