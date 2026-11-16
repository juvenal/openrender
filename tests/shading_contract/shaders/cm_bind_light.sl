/* Spec 033 characterisation (BindSpaces_Characterise, tests/shading_contract). Parameter defaults written in every
   coordinate system a bind-time init can name: "current", "shader", "object", "light", "world", "camera", "NDC",
   "raster", "screen" and a system the scene declares ("cmuser"), as a point, a vector and a normal (a matrix default in a named space is not recorded: the .rslo loader rejects it, see validation.md section 4). They are
   evaluated once, when the shader is bound (the init section), under the interpreter and under the JIT; the test prints
   the stored defaults bit for bit. An unknown name is deliberately absent: the interpreter's answer is undefined today
   (research R6). */
light cm_bind_light(
    point p_current = point "current" (1, 2, 3);
    vector v_current = vector "current" (0.25, 0.5, 0.75);
    normal n_current = normal "current" (0, 1, 0);
    point p_shader = point "shader" (1, 2, 3);
    vector v_shader = vector "shader" (0.25, 0.5, 0.75);
    normal n_shader = normal "shader" (0, 1, 0);
    point p_object = point "object" (1, 2, 3);
    vector v_object = vector "object" (0.25, 0.5, 0.75);
    normal n_object = normal "object" (0, 1, 0);
    point p_light = point "light" (1, 2, 3);
    vector v_light = vector "light" (0.25, 0.5, 0.75);
    normal n_light = normal "light" (0, 1, 0);
    point p_world = point "world" (1, 2, 3);
    vector v_world = vector "world" (0.25, 0.5, 0.75);
    normal n_world = normal "world" (0, 1, 0);
    point p_camera = point "camera" (1, 2, 3);
    vector v_camera = vector "camera" (0.25, 0.5, 0.75);
    normal n_camera = normal "camera" (0, 1, 0);
    point p_NDC = point "NDC" (1, 2, 3);
    vector v_NDC = vector "NDC" (0.25, 0.5, 0.75);
    normal n_NDC = normal "NDC" (0, 1, 0);
    point p_raster = point "raster" (1, 2, 3);
    vector v_raster = vector "raster" (0.25, 0.5, 0.75);
    normal n_raster = normal "raster" (0, 1, 0);
    point p_screen = point "screen" (1, 2, 3);
    vector v_screen = vector "screen" (0.25, 0.5, 0.75);
    normal n_screen = normal "screen" (0, 1, 0);
    point p_cmuser = point "cmuser" (1, 2, 3);
    vector v_cmuser = vector "cmuser" (0.25, 0.5, 0.75);
    normal n_cmuser = normal "cmuser" (0, 1, 0))
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
