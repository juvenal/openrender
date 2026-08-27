/* T008b -- FR-006 discrimination coverage: a uniform-classified instruction
 * lexically nested inside a varying conditional whose predicate is false at
 * the leading shading points.
 *
 * `bias = bias + 1` is uniform-classified (dst/src/literal strides all 0,
 * confirmed via the T003d LLVM-IR dump mechanism against this exact shader
 * body: `call void @op_addff(ptr %16, i32 0, ptr %18, i32 0, ptr %lit2,
 * i32 0, i32 %0, ptr %2)`) even though it sits inside `if (u > 0.05)`, a
 * genuinely varying conditional. Per contracts/op-uniform-collapse.md
 * SS2.2/SS2.3, a uniform-classified instruction must execute exactly once,
 * unconditionally -- mirroring the interpreter's
 * `if (code->uniform) { expr; ... }` arm (the "once, no tag test" case),
 * which ignores the enclosing conditional's active mask entirely.
 *
 * `u > 0.05` is chosen specifically to be FALSE at the sphere's
 * first-diced shading points (REYES grid dicing starts at the (u=0, v=0)
 * grid corner), not true there. This is deliberate: a collapse
 * implementation that violates SS2.3's prohibition by narrowing `n` to 1
 * while still passing the enclosing `if`'s live (non-null) tags pointer
 * would read `ACTIVE(tags, 0)` -- this `if`'s per-point active mask at
 * index 0 -- which is false here, since the leading shading points did not
 * take the branch, silently skipping the uniform write. Had the predicate
 * been true at the leading points instead, that same bug would
 * accidentally render correctly for this scene, defeating the test.
 *
 * Expected image: the uniform write to `bias` executes exactly once,
 * unconditionally, regardless of `u`, so the correct render is a
 * uniformly-lit sphere (Ci = Cs * 1, i.e. Cs unchanged). A SS2.3-violating
 * collapse instead renders Ci = Cs * 0 (black), since the uniform write to
 * `bias` never happens.
 *
 * `Oi = 1;` is required here for a reason unrelated to the above: the JIT
 * (unlike the interpreter) does not default Oi to opaque when a shader
 * never assigns it, so any .slo shader that omits this line renders fully
 * transparent -- compositing to solid black regardless of Ci -- which would
 * make this probe fail for a reason having nothing to do with FR-006's
 * uniform-collapse behavior. See measurements.md's T008b section for the
 * empirical trace that found this.
 */
surface uniform_in_conditional_probe()
{
    uniform float bias = 0;

    Oi = 1;

    if (u > 0.05) {
        bias = bias + 1;
    }

    Ci = Cs * bias;
}
