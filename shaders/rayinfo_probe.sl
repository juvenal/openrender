/* GitHub issue #3 (spec 017, US3) regression: rayinfo() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode
 * as visibility()/occlusion() etc. Exercises all 5 query strings
 * ("label", "depth", "origin", "direction", "length"). Note the
 * destination-type ambiguity documented in jitRayInfo's header comment
 * (shading.h): "label" needs a `string` destination, the other four need
 * a numeric one -- there is no way to query both a string and a numeric
 * result from a single rayinfo() call, so this probe uses two separate
 * calls for "label" vs the rest.
 *
 * `labelOk`/`depthOk`/`directionOk`/`lengthOk`/`allFoundOk`/`allOk` are
 * declared `varying` (not `uniform`) even though their values are
 * uniform in practice: GitHub issue #8 (see
 * determinant_distance_probe.sl's header comment) makes an assignment
 * inside an `if` body execute unconditionally whenever both the
 * condition and the assigned variable are uniform -- which would make
 * every one of these checks pass regardless of whether rayinfo()
 * actually returned the right values. A varying destination routes
 * around that by forcing the JIT to consult the real per-vertex
 * active-mask instead of the buggy null-tags fast path. `labelOk`
 * specifically also can't be replaced with a raw-value comparison
 * (string equality), same as raylabel_probe.sl/shadername_probe.sl.
 */
surface rayinfo_probe()
{
    uniform string labelOut = "";
    uniform float labelFound = rayinfo("label", labelOut);
    varying float labelOk = 0;
    if (labelOut == "camera") labelOk = 1;

    uniform float depthOut = -1;
    uniform float depthFound = rayinfo("depth", depthOut);
    varying float depthOk = 0;
    if (depthOut == 0) depthOk = 1;

    uniform vector originOut = vector(0, 0, 0);
    uniform float originFound = rayinfo("origin", originOut);

    uniform vector directionOut = vector(0, 0, 0);
    uniform float directionFound = rayinfo("direction", directionOut);
    uniform float directionLen = length(directionOut);
    varying float directionOk = 0;
    if (directionLen > 0.99 && directionLen < 1.01) directionOk = 1;

    uniform float lengthOut = 0;
    uniform float lengthFound = rayinfo("length", lengthOut);
    varying float lengthOk = 0;
    if (lengthOut > 0) lengthOk = 1;

    uniform float allFound = labelFound + depthFound + originFound + directionFound + lengthFound;
    varying float allFoundOk = 0;
    if (allFound == 5) allFoundOk = 1;

    varying float allOk = labelOk + depthOk + directionOk + lengthOk + allFoundOk;

    Ci = color(allOk / 5, originFound, 0.5);
}
