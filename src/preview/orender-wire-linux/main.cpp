// orender-wire — Linux GTK 4 / OpenGL 3.3 Core wireframe scene previewer.

#include <adwaita.h>
#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>

#include "ribpreview_api.h"
#include "libribpreview/cameraExport.h"
#include "arcball.h"

// ─── GLSL 3.30 core shaders ──────────────────────────────────────────────────

static const char *SCENE_VERT = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 inColor;
uniform mat4 mvp;
out vec3 vColor;
void main() { gl_Position = mvp * vec4(position, 1.0); vColor = inColor; }
)";

static const char *SCENE_FRAG = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

static const char *GRID_VERT = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
uniform mat4 mvp;
uniform vec3 gridOrigin;
out vec3 vColor;
void main() { gl_Position = mvp * vec4(position + gridOrigin, 1.0); vColor = color; }
)";

static const char *GRID_FRAG = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

// ─── GL helpers ──────────────────────────────────────────────────────────────

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        fprintf(stderr, "orender-wire: shader compile error: %s\n", log);
    }
    return s;
}

static GLuint link_program(const char *vert, const char *frag) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        fprintf(stderr, "orender-wire: shader link error: %s\n", log);
    }
    return p;
}

// ─── Grid/axis geometry ──────────────────────────────────────────────────────

struct GridVert { float x,y,z,r,g,b; };

static std::vector<GridVert> buildGridAxis() {
    std::vector<GridVert> v;
    v.reserve(90);
    constexpr float gc = 0.28f;
    for (int i = -10; i <= 10; i++) {
        float f = (float)i;
        v.push_back({-10,0,f, gc,gc,gc}); v.push_back({ 10,0,f, gc,gc,gc});
        v.push_back({ f,0,-10,gc,gc,gc}); v.push_back({ f,0,10, gc,gc,gc});
    }
    // XYZ gizmo
    v.push_back({0,0,0,1.f,0.2f,0.2f}); v.push_back({2,0,0,1.f,0.2f,0.2f});
    v.push_back({0,0,0,0.2f,1.f,0.2f}); v.push_back({0,2,0,0.2f,1.f,0.2f});
    v.push_back({0,0,0,0.2f,0.2f,1.f}); v.push_back({0,0,2,0.2f,0.2f,1.f});
    return v;
}

// ─── App state ───────────────────────────────────────────────────────────────

struct AppState {
    const char    *ribPath;

    GtkWidget     *glArea;
    GtkWidget     *spinner;

    // GL resources (created on realize, after GL context exists)
    GLuint sceneProg  = 0, gridProg      = 0;
    GLuint sceneVAO   = 0, sceneVBO      = 0, sceneColorVBO = 0;
    GLuint gridVAO    = 0, gridVBO       = 0;
    int    sceneCount = 0, gridCount     = 0;

    ArcballCamera *arcball = nullptr;

    // Input state
    bool  orbitActive = false;
    bool  panActive   = false;
    double pressX = 0, pressY = 0;

    // Geometry available once background load completes.
    PreviewSceneC *scene = nullptr;
};

// ─── Background load (GTask) ─────────────────────────────────────────────────

static void load_scene_thread(GTask *task, gpointer, gpointer task_data, GCancellable *) {
    const char *path = static_cast<const char *>(task_data);
    PreviewSceneC *scene = ribpreview_load(path);
    if (!scene) {
        g_task_return_error(task, g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED,
                                               "ribpreview_load failed"));
        return;
    }
    g_task_return_pointer(task, scene, nullptr);
}

// ─── GL realize ──────────────────────────────────────────────────────────────

static void on_realize(GtkGLArea *area, AppState *state) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) return;

    state->sceneProg = link_program(SCENE_VERT, SCENE_FRAG);
    state->gridProg  = link_program(GRID_VERT,  GRID_FRAG);

    // Scene VAO/VBO (filled later when scene loads)
    glGenVertexArrays(1, &state->sceneVAO);
    glGenBuffers(1, &state->sceneVBO);
    glGenBuffers(1, &state->sceneColorVBO);

    glBindVertexArray(state->sceneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->sceneVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, state->sceneColorVBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Grid+axis VAO/VBO (static)
    auto gridVerts = buildGridAxis();
    state->gridCount = (int)gridVerts.size();

    glGenVertexArrays(1, &state->gridVAO);
    glGenBuffers(1, &state->gridVBO);
    glBindVertexArray(state->gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(gridVerts.size() * sizeof(GridVert)),
                 gridVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVert), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridVert), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

// ─── GL unrealize ────────────────────────────────────────────────────────────

static void on_unrealize(GtkGLArea *area, AppState *state) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) return;

    glDeleteVertexArrays(1, &state->sceneVAO);
    glDeleteBuffers(1, &state->sceneVBO);
    glDeleteBuffers(1, &state->sceneColorVBO);
    glDeleteVertexArrays(1, &state->gridVAO);
    glDeleteBuffers(1, &state->gridVBO);
    glDeleteProgram(state->sceneProg);
    glDeleteProgram(state->gridProg);
}

// ─── GL render ───────────────────────────────────────────────────────────────

static gboolean on_render(GtkGLArea * /*area*/, GdkGLContext *, AppState *state) {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!state->arcball) return TRUE;

    float mvp[16];
    state->arcball->viewProjectionMatrix(mvp);

    // Scene wire pass
    if (state->sceneCount > 0) {
        glUseProgram(state->sceneProg);
        glUniformMatrix4fv(glGetUniformLocation(state->sceneProg, "mvp"),
                           1, GL_FALSE, mvp);
        glBindVertexArray(state->sceneVAO);
        glDrawArrays(GL_LINES, 0, state->sceneCount);
        glBindVertexArray(0);
    }

    // Grid + axis pass
    vec3 go = state->arcball->gridOriginWorld();
    float gridOrigin[3] = {go.x, go.y, go.z};
    glUseProgram(state->gridProg);
    glUniformMatrix4fv(glGetUniformLocation(state->gridProg, "mvp"),
                       1, GL_FALSE, mvp);
    glUniform3fv(glGetUniformLocation(state->gridProg, "gridOrigin"),
                 1, gridOrigin);
    glBindVertexArray(state->gridVAO);
    glDrawArrays(GL_LINES, 0, state->gridCount);
    glBindVertexArray(0);

    return TRUE;
}

// ─── GL resize ───────────────────────────────────────────────────────────────

static void on_resize(GtkGLArea *, int width, int height, AppState *state) {
    glViewport(0, 0, width, height);
    if (state->arcball)
        state->arcball->updateAspect((float)width, (float)height);
}

// ─── Upload scene geometry after load ────────────────────────────────────────

static void upload_scene(AppState *state) {
    int n = state->scene->vertexCount;
    if (n <= 0) return;

    gtk_gl_area_make_current(GTK_GL_AREA(state->glArea));

    glBindBuffer(GL_ARRAY_BUFFER, state->sceneVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(n * 3 * sizeof(float)),
                 state->scene->vertices, GL_STATIC_DRAW);

    const float *colPtr = state->scene->colors;
    std::vector<float> fallback;
    if (!colPtr) {
        fallback.assign((size_t)n * 3, 0.85f);
        colPtr = fallback.data();
    }
    glBindBuffer(GL_ARRAY_BUFFER, state->sceneColorVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(n * 3 * sizeof(float)),
                 colPtr, GL_STATIC_DRAW);

    state->sceneCount = n;
}

// ─── Background load completion (main thread) ─────────────────────────────────

static void on_load_done(GObject *, GAsyncResult *res, gpointer user_data) {
    AppState *state = static_cast<AppState *>(user_data);

    GError *err = nullptr;
    PreviewSceneC *scene = static_cast<PreviewSceneC *>(
        g_task_propagate_pointer(G_TASK(res), &err));

    // Hide spinner regardless of outcome.
    if (state->spinner)
        gtk_widget_set_visible(state->spinner, FALSE);

    if (err || !scene) {
        fprintf(stderr, "orender-wire: error: RIB parse failed (see above)\n");
        if (err) g_error_free(err);
        // Leave blank window; user can close.
        return;
    }

    state->scene = scene;

    const PreviewCameraC &cam  = scene->camera;
    const PreviewBoundsC &bnds = scene->bounds;

    int w = gtk_widget_get_width(state->glArea);
    int h = gtk_widget_get_height(state->glArea);
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    state->arcball = new ArcballCamera(
        cam.projMatrix, cam.viewMatrix,
        bnds.sceneBoundsMin, bnds.sceneBoundsMax,
        (float)w, (float)h);

    upload_scene(state);

    gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
}

// ─── Input controllers ────────────────────────────────────────────────────────

static void on_press(GtkGestureClick *gesture, int /*n_press*/,
                     double x, double y, AppState *state) {
    if (!state->arcball) return;
    int button = gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(gesture));
    double sx = x, sy = y;
    if (button == 1) {
        state->orbitActive = true;
        state->panActive   = false;
        state->arcball->beginOrbit((float)sx, (float)sy);
    } else if (button == 2) {
        state->panActive   = true;
        state->orbitActive = false;
        state->arcball->beginPan((float)sx, (float)sy);
    }
    state->pressX = x;
    state->pressY = y;
}

static void on_release(GtkGestureClick *, int, double, double, AppState *state) {
    state->orbitActive = false;
    state->panActive   = false;
}

static void on_motion(GtkEventControllerMotion *, double x, double y, AppState *state) {
    if (!state->arcball) return;
    if (state->orbitActive) {
        state->arcball->orbit((float)x, (float)y);
        gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
    } else if (state->panActive) {
        state->arcball->pan((float)x, (float)y);
        gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
    }
}

static gboolean on_scroll(GtkEventControllerScroll *, double /*dx*/, double dy,
                           AppState *state) {
    if (!state->arcball) return FALSE;
    state->arcball->zoom((float)(dy > 0 ? 1 : -1));
    gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
    return TRUE;
}

// ─── Camera export (S key) ────────────────────────────────────────────────────

static void save_camera_callback(GObject *dialog, GAsyncResult *result,
                                 gpointer user_data) {
    AppState *state = static_cast<AppState *>(user_data);
    GError *err = nullptr;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(dialog), result, &err);
    if (!file) { if (err) g_error_free(err); return; }

    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (!path) return;

    float c2w[16];
    state->arcball->cameraToWorldMatrix(c2w);

    // Transpose column-major to row-major for ribcam_write.
    float row[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            row[r*4+c] = c2w[c*4+r];

    CameraExport cam{};
    std::memcpy(cam.cameraToWorld, row, sizeof(row));
    cam.projectionType = state->arcball->isOrthographic() ? 1 : 0;
    cam.fov            = state->arcball->fovDegrees();
    cam.outputPath     = path;

    bool exists = std::filesystem::exists(path);
    cam.updateExisting = exists;
    bool ok = exists ? replaceRibCamera(cam) : writeRibCamera(cam);

    if (!ok)
        fprintf(stderr, "orender-wire: failed to save camera to '%s'\n", path);

    g_free(path);
}

static gboolean on_key(GtkEventControllerKey *, guint keyval, guint /*keycode*/,
                        GdkModifierType, AppState *state) {
    switch (keyval) {
    case GDK_KEY_r:
    case GDK_KEY_R:
    case GDK_KEY_Home:
        if (state->arcball) {
            state->arcball->reset();
            gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
        }
        return TRUE;
    case GDK_KEY_s:
    case GDK_KEY_S:
        if (state->arcball) {
            GtkWindow *win = GTK_WINDOW(
                gtk_widget_get_ancestor(state->glArea, GTK_TYPE_WINDOW));
            GtkFileDialog *dlg = gtk_file_dialog_new();
            gtk_file_dialog_set_title(dlg, "Export Camera");
            gtk_file_dialog_set_initial_name(dlg, "camera.rib");
            gtk_file_dialog_save(dlg, win, nullptr,
                                 save_camera_callback, state);
            g_object_unref(dlg);
        }
        return TRUE;
    case GDK_KEY_Escape:
    case GDK_KEY_q:
    case GDK_KEY_Q: {
        GtkWindow *win = GTK_WINDOW(
            gtk_widget_get_ancestor(state->glArea, GTK_TYPE_WINDOW));
        if (win) gtk_window_close(win);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

// ─── Window close (clean shutdown) ───────────────────────────────────────────

static gboolean on_close_request(GtkWindow *, AppState *state) {
    if (state->scene) {
        ribpreview_free(state->scene);
        state->scene = nullptr;
    }
    delete state->arcball;
    state->arcball = nullptr;
    return FALSE;   // allow default close
}

// ─── GTK application activate ─────────────────────────────────────────────────

static void on_activate(AdwApplication *app, gpointer user_data) {
    AppState *state = static_cast<AppState *>(user_data);

    // Window
    GtkWidget *window = adw_application_window_new(GTK_APPLICATION(app));
    std::string title = std::string("orender-wire — ")
                      + std::filesystem::path(state->ribPath).filename().string();
    gtk_window_set_title(GTK_WINDOW(window), title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "close-request",
                     G_CALLBACK(on_close_request), state);

    // Layout
    GtkWidget *toolbar_view = adw_toolbar_view_new();
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), toolbar_view);

    GtkWidget *header_bar = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_bar);

    // Overlay: glArea + spinner
    GtkWidget *overlay = gtk_overlay_new();
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), overlay);

    GtkWidget *glArea = gtk_gl_area_new();
    state->glArea = glArea;
    gtk_gl_area_set_required_version(GTK_GL_AREA(glArea), 3, 3);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(glArea), TRUE);
    gtk_widget_set_hexpand(glArea, TRUE);
    gtk_widget_set_vexpand(glArea, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), glArea);

    GtkWidget *spinner = gtk_spinner_new();
    state->spinner = spinner;
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(spinner, 48, 48);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), spinner);

    // GL signals
    g_signal_connect(glArea, "realize",   G_CALLBACK(on_realize),   state);
    g_signal_connect(glArea, "unrealize", G_CALLBACK(on_unrealize),  state);
    g_signal_connect(glArea, "render",    G_CALLBACK(on_render),     state);
    g_signal_connect(glArea, "resize",    G_CALLBACK(on_resize),     state);

    // Input controllers
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0); // all buttons
    g_signal_connect(click, "pressed",  G_CALLBACK(on_press),   state);
    g_signal_connect(click, "released", G_CALLBACK(on_release), state);
    gtk_widget_add_controller(glArea, GTK_EVENT_CONTROLLER(click));

    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(
        gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion), state);
    gtk_widget_add_controller(glArea, GTK_EVENT_CONTROLLER(motion));

    GtkEventControllerScroll *scroll = GTK_EVENT_CONTROLLER_SCROLL(
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL));
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), state);
    gtk_widget_add_controller(glArea, GTK_EVENT_CONTROLLER(scroll));

    GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY(
        gtk_event_controller_key_new());
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key), state);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(key));

    gtk_window_present(GTK_WINDOW(window));

    // Background RIB load
    GTask *task = g_task_new(nullptr, nullptr, on_load_done, state);
    g_task_set_task_data(task, (gpointer)state->ribPath, nullptr);
    g_task_run_in_thread(task, load_scene_thread);
    g_object_unref(task);
}

// ─── main ─────────────────────────────────────────────────────────────────────

static const char *HELP_TEXT =
"Usage: orender-wire [OPTIONS] <scene.rib>\n"
"\n"
"Open and inspect a RenderMan RIB scene as an interactive wireframe.\n"
"\n"
"Options:\n"
"  --help       Show this help text and exit\n"
"  --version    Print version and exit\n"
"\n"
"Controls:\n"
"  Left drag      Orbit camera\n"
"  Middle drag    Pan camera\n"
"  Scroll         Zoom\n"
"  R / Home       Reset camera to RIB viewpoint\n"
"  S              Save current camera to RIB file\n"
"  Q / Escape     Quit\n"
"\n"
"Exit codes:\n"
"  0  Normal exit\n"
"  1  Usage error (bad arguments)\n"
"  2  File not found or unreadable\n"
"  3  RIB parse failed\n"
"  4  No display available\n";

int main(int argc, char **argv) {
    // Suppress Mesa/EGL warnings and driver loader errors by forcing the OpenGL 
    // renderer and silencing Mesa's internal diagnostic/error logging.
    setenv("GSK_RENDERER", "gl", 0);
    setenv("EGL_LOG_LEVEL", "fatal", 0);
    setenv("MESA_DEBUG", "silent", 0);
    setenv("LIBGL_DEBUG", "quiet", 0);
    setenv("MESA_LOG_FILE", "/dev/null", 0);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fputs(HELP_TEXT, stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            puts("orender-wire 1.0");
            return 0;
        }
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: orender-wire <scene.rib>\n");
        return 1;   // exit 1: usage error
    }

    const char *ribPath = argv[1];
    if (access(ribPath, R_OK) != 0) {
        fprintf(stderr, "orender-wire: cannot open '%s': %s\n",
                ribPath, strerror(errno));
        return 2;
    }

    // Release the terminal. nochdir=1 to keep CWD for RIB resources, 
    // noclose=1 to keep stderr open for warnings.
    if (daemon(1, 1) != 0) {
        perror("orender-wire: daemon failed");
        return 5;
    }

    AppState state{};
    state.ribPath = ribPath;

    AdwApplication *app = adw_application_new("press.v2labs.orender.wire",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &state);

    int status = g_application_run(G_APPLICATION(app), 0, nullptr);
    g_object_unref(app);

    // No display → GTK exits with non-zero; map to exit code 4.
    if (status != 0) return 4;
    return 0;
}
