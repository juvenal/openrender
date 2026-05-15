# Developer Notes

## Project Status

| Area | Status | Detail File |
|------|--------|-------------|
| oshader compiler | Complete — IR, optimization passes, `.rslo` | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| Imager shaders | Complete — all 7 spec variables, thread-safe | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| Framebuffer — macOS | Complete — IPC helper, multi-window, TTY fix | [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) |
| Framebuffer — Linux | Architecture done; **untested** | [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) |
| Geometry statements | Complete — in-place expansion, circularity detection | [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) |
| Hider parity | Partial — filtering and jitter done; motion blur, transparency pending | [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) |
| RISpec 3.2 gaps | 1 of 7 implemented | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
| C++20 / C17 migration | Phase 2 complete — portable I/O, binary security; Phase 3 future | [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) |

## Open Issues

- [ ] Purging tessellations for raytracing (no cache eviction mechanism found)
- [ ] Moving raytraced surface (`CRaytracer` lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [ ] Framebuffer Linux migration — requires Linux build to validate end-to-end
- [ ] Irradiance accuracy issues

## Todos

- [ ] OpenEXR input for textures (output is supported; input/texture reading is missing)
- [ ] Trace subsets (`trace()` does not yet filter by subset)
- [ ] Patch crack stitching (currently handled via displacement bounds)
- [ ] Hider parity completion — see [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md)
- [ ] LLVM integration and binary shader compilation — see [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md)

## See Also

| File | Coverage |
|------|----------|
| [BUGS.md](DEVNOTES_DETAILS/BUGS.md) | Full open issues and resolved bugs with fix notes |
| [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) | RenderMan Spec 3.2 compliance gaps |
| [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) | Shader compiler and imager shader implementation |
| [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) | Framebuffer IPC display architecture |
| [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) | Geometry RIB statement implementation |
| [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) | Stochastic vs. raytrace hider alignment |
| [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) | C++20/C17 migration, portable I/O, binary format changes |
