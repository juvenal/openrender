// orender-wire — Linux GTK 4 / OpenGL 3.3 Core wireframe scene previewer.

#include <adwaita.h>
#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// Points need an explicit size -- GL_POINTS renders 1-pixel dots unless the vertex shader
// writes gl_PointSize itself (with GL_PROGRAM_POINT_SIZE enabled). SCENE_VERT/SCENE_FRAG above
// are reused as-is for triangles (discs arrive already CPU-expanded into the triangle buffer --
// see diskExpand.h) by simply drawing GL_TRIANGLES instead of GL_LINES; only points need a
// distinct pipeline.
static const char *POINT_VERT = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 inColor;
uniform mat4 mvp;
out vec3 vColor;
void main() { gl_Position = mvp * vec4(position, 1.0); vColor = inColor; gl_PointSize = 4.0; }
)";

static const char *POINT_FRAG = R"(
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
    GLuint sceneProg  = 0, gridProg      = 0, pointsProg = 0;
    GLuint sceneVAO   = 0, sceneVBO      = 0, sceneColorVBO = 0;
    GLuint gridVAO    = 0, gridVBO       = 0;
    int    sceneCount = 0, gridCount     = 0;

    // Data-document buffers (spec 016). Populated only when opening a data document (photon
    // map, cache, point cloud, brick map, debug dump) instead of a RIB scene; a RIB document
    // leaves all three counts at 0 and rendering skips them. Discs arrive already CPU-expanded
    // into the triangle arrays (see diskExpand.h), so triangles reuses sceneProg/sceneVAO's
    // layout via its own VAO -- no separate disc pipeline is needed.
    GLuint dataLineVAO = 0, dataLineVBO = 0, dataLineColorVBO = 0;
    GLuint dataPointVAO = 0, dataPointVBO = 0, dataPointColorVBO = 0;
    GLuint dataTriVAO = 0, dataTriVBO = 0, dataTriColorVBO = 0;
    int    dataLineCount = 0, dataPointCount = 0, dataTriCount = 0;

    // Retained (not closed on load) because ribdata_key()'s re-emit cycle (User Story 2) needs
    // the CDataView underneath it to stay alive for the document's whole session -- unlike
    // ribpreview_free's fully-materialized-then-torn-down RIB scene. Closed in on_close_request.
    RibDataDocument *dataDoc = nullptr;

    // Header-bar menu (User Story 2): one GSimpleAction per legacy key, shared between the menu
    // button and the existing on_key handler (one action implementation, two entry points).
    // Enabled/disabled per-document-type in update_header_bar_state().
    GSimpleAction *actPrevChannel = nullptr, *actNextChannel = nullptr;
    GSimpleAction *actIncDetail = nullptr, *actDecDetail = nullptr;
    GSimpleAction *actDrawBoxes = nullptr, *actDrawDiscs = nullptr, *actDrawPoints = nullptr;
    GSimpleAction *actSaveCamera = nullptr;
    AdwWindowTitle *windowTitle = nullptr;

    ArcballCamera *arcball = nullptr;

    // Input state
    bool  orbitActive = false;
    bool  panActive   = false;
    double pressX = 0, pressY = 0;

    // Geometry available once background load completes.
    PreviewSceneC *scene = nullptr;
};

// Result of the background open: exactly one of ribScene/dataDoc is set on success. A plain
// struct with raw pointers -- ownership of whichever pointer is set transfers to AppState in
// on_load_done, so ~LoadResult must never free them itself.
struct LoadResult {
    bool isData = false;
    PreviewSceneC *ribScene = nullptr;
    RibDataDocument *dataDoc = nullptr;
};

// ─── Background load (GTask) ─────────────────────────────────────────────────

static void load_scene_thread(GTask *task, gpointer, gpointer task_data, GCancellable *) {
    const char *path = static_cast<const char *>(task_data);
    LoadResult *result = new LoadResult();

    // Content-based detection (FR-001): -1 means no data-file magic at all (try RIB); any other
    // value -- including -2, "recognized magic but incompatible version/word-size" -- means this
    // IS a data file and must be routed to ribdata_open(), which reports the real failure reason
    // rather than falling back to RIB and silently rendering an empty scene. Mirrors
    // wireCliRun()'s auto-detection in src/preview/libribpreview/wireCli.cpp.
    if (ribdata_sniff(path) != -1) {
        result->isData = true;
        int err = 0;
        result->dataDoc = ribdata_open(path, &err);
        if (!result->dataDoc) {
            delete result;
            g_task_return_error(task, g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                   "not a recognized data file"));
            return;
        }
    } else {
        result->ribScene = ribpreview_load(path);
        if (!result->ribScene) {
            delete result;
            g_task_return_error(task, g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED,
                                                   "ribpreview_load failed"));
            return;
        }
    }

    g_task_return_pointer(task, result, [](gpointer p) { delete static_cast<LoadResult *>(p); });
}

// ─── GL realize ──────────────────────────────────────────────────────────────

static void on_realize(GtkGLArea *area, AppState *state) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) return;

    state->sceneProg  = link_program(SCENE_VERT, SCENE_FRAG);
    state->gridProg   = link_program(GRID_VERT,  GRID_FRAG);
    state->pointsProg = link_program(POINT_VERT, POINT_FRAG);

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

    // Data-document VAO/VBOs (lines, points, triangles -- filled later, same packed-float3
    // layout as the scene VAO above).
    auto setupPrimVAO = [](GLuint &vao, GLuint &vbo, GLuint &colorVbo) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &colorVbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, colorVbo);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    };
    setupPrimVAO(state->dataLineVAO, state->dataLineVBO, state->dataLineColorVBO);
    setupPrimVAO(state->dataPointVAO, state->dataPointVBO, state->dataPointColorVBO);
    setupPrimVAO(state->dataTriVAO, state->dataTriVBO, state->dataTriColorVBO);

    // Data-document geometry (brick-map boxes, expanded discs, etc.) has no reliable winding
    // order to cull against -- discs in particular are single-sided fans with an arbitrary
    // basis (see diskExpand.h). GL_CULL_FACE defaults to disabled, but state this explicitly
    // rather than relying on the default.
    glDisable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);

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
    glDeleteVertexArrays(1, &state->dataLineVAO);
    glDeleteBuffers(1, &state->dataLineVBO);
    glDeleteBuffers(1, &state->dataLineColorVBO);
    glDeleteVertexArrays(1, &state->dataPointVAO);
    glDeleteBuffers(1, &state->dataPointVBO);
    glDeleteBuffers(1, &state->dataPointColorVBO);
    glDeleteVertexArrays(1, &state->dataTriVAO);
    glDeleteBuffers(1, &state->dataTriVBO);
    glDeleteBuffers(1, &state->dataTriColorVBO);
    glDeleteVertexArrays(1, &state->gridVAO);
    glDeleteBuffers(1, &state->gridVBO);
    glDeleteProgram(state->sceneProg);
    glDeleteProgram(state->gridProg);
    glDeleteProgram(state->pointsProg);
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

    // Data-document passes (lines, triangles -- including expanded discs -- reuse sceneProg;
    // points need pointsProg for gl_PointSize).
    if (state->dataLineCount > 0) {
        glUseProgram(state->sceneProg);
        glUniformMatrix4fv(glGetUniformLocation(state->sceneProg, "mvp"),
                           1, GL_FALSE, mvp);
        glBindVertexArray(state->dataLineVAO);
        glDrawArrays(GL_LINES, 0, state->dataLineCount);
        glBindVertexArray(0);
    }

    if (state->dataTriCount > 0) {
        glUseProgram(state->sceneProg);
        glUniformMatrix4fv(glGetUniformLocation(state->sceneProg, "mvp"),
                           1, GL_FALSE, mvp);
        glBindVertexArray(state->dataTriVAO);
        glDrawArrays(GL_TRIANGLES, 0, state->dataTriCount);
        glBindVertexArray(0);
    }

    if (state->dataPointCount > 0) {
        glUseProgram(state->pointsProg);
        glUniformMatrix4fv(glGetUniformLocation(state->pointsProg, "mvp"),
                           1, GL_FALSE, mvp);
        glBindVertexArray(state->dataPointVAO);
        glDrawArrays(GL_POINTS, 0, state->dataPointCount);
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

// Uploads one PrimArrayC's verts/cols into a pair of already-created VBOs, or leaves the count
// at 0 if empty -- calling glBufferData with a null/empty pointer is undefined, not a no-op, so
// every caller must check count first.
static void upload_prim_array(GLuint vbo, GLuint colorVbo, const PrimArrayC &arr, int &countOut) {
    int n = arr.count;
    if (n <= 0 || !arr.verts || !arr.cols) { countOut = 0; return; }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(n * 3 * sizeof(float)), arr.verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, colorVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(n * 3 * sizeof(float)), arr.cols, GL_STATIC_DRAW);
    countOut = n;
}

static void upload_data_scene(AppState *state, const DataSceneC *snap) {
    gtk_gl_area_make_current(GTK_GL_AREA(state->glArea));
    upload_prim_array(state->dataLineVBO, state->dataLineColorVBO, snap->lines, state->dataLineCount);
    upload_prim_array(state->dataPointVBO, state->dataPointColorVBO, snap->points, state->dataPointCount);
    upload_prim_array(state->dataTriVBO, state->dataTriColorVBO, snap->triangles, state->dataTriCount);
}

// ─── Header-bar / on-screen state (User Story 2) ──────────────────────────────

// Mirrors wireCli.cpp's drawModeName(): boxes/discs/points for a brick map, discs/points for a
// point cloud, "fixed" for the three document types with no user-controllable draw mode.
static const char *draw_mode_display_name(RibDataType type, int mode) {
    if (type == RIBDATA_TYPE_BRICKMAP) {
        switch (mode) {
        case 0: return "Boxes";
        case 1: return "Discs";
        default: return "Points";
        }
    }
    if (type == RIBDATA_TYPE_POINTCLOUD)
        return (mode == 1) ? "Discs" : "Points";
    return "Fixed";
}

// Refreshes the AdwWindowTitle subtitle (FR-016: channel/detail/draw-mode visible on screen, not
// only in a terminal) and each header-bar action's enabled state (T061: document-type-
// conditional controls; FR-017/T062: no inert channel control when numChannels == 0). Called
// once after a document finishes loading and again after every key/menu action that changes
// state.
static void update_header_bar_state(AppState *state) {
    bool hasData = state->dataDoc != nullptr;
    bool hasChannels = false, isBrickmap = false, isPointcloud = false;
    std::string subtitle;

    if (hasData) {
        const DataSceneC *snap = ribdata_snapshot(state->dataDoc);
        hasChannels  = snap->numChannels > 0;
        isBrickmap   = snap->documentType == RIBDATA_TYPE_BRICKMAP;
        isPointcloud = snap->documentType == RIBDATA_TYPE_POINTCLOUD;

        std::vector<std::string> parts;
        if (hasChannels) {
            const char *name = ribdata_channel_name(state->dataDoc, snap->currentChannel);
            parts.push_back(std::string("Channel: ") + (name ? name : ""));
        }
        if (isBrickmap)
            parts.push_back("Detail: " + std::to_string(snap->detailLevel));
        parts.push_back(std::string("Draw: ") + draw_mode_display_name(snap->documentType, snap->drawMode));

        for (size_t i = 0; i < parts.size(); i++) {
            if (i > 0) subtitle += "   \xE2\x80\xA2   "; // U+2022 BULLET
            subtitle += parts[i];
        }
    }

    if (state->windowTitle)
        adw_window_title_set_subtitle(state->windowTitle, subtitle.c_str());

    bool supportsDetail     = isBrickmap;
    bool supportsBoxMode    = isBrickmap;
    bool supportsDrawToggle = isBrickmap || isPointcloud;

    if (state->actPrevChannel) g_simple_action_set_enabled(state->actPrevChannel, hasChannels);
    if (state->actNextChannel) g_simple_action_set_enabled(state->actNextChannel, hasChannels);
    if (state->actIncDetail)   g_simple_action_set_enabled(state->actIncDetail, supportsDetail);
    if (state->actDecDetail)   g_simple_action_set_enabled(state->actDecDetail, supportsDetail);
    if (state->actDrawBoxes)   g_simple_action_set_enabled(state->actDrawBoxes, supportsBoxMode);
    if (state->actDrawDiscs)   g_simple_action_set_enabled(state->actDrawDiscs, supportsDrawToggle);
    if (state->actDrawPoints)  g_simple_action_set_enabled(state->actDrawPoints, supportsDrawToggle);
    // T061: hide (disable) Save Camera when a data document is open.
    if (state->actSaveCamera)  g_simple_action_set_enabled(state->actSaveCamera, !hasData);
}

// Applies one legacy key (`m l b d p q w`) to the open data document -- shared by on_key and the
// header-bar menu actions (one action implementation, two entry points). No-op if no data
// document is open, or if the underlying CDataView doesn't recognize this key for its type (e.g.
// 'b' on a point cloud) -- ribdata_key()/keyDown() already handle that gracefully.
static void apply_data_key(AppState *state, char key) {
    if (!state->dataDoc) return;
    if (!ribdata_key(state->dataDoc, (int)key)) return;

    const DataSceneC *snap = ribdata_snapshot(state->dataDoc);
    upload_data_scene(state, snap);
    update_header_bar_state(state);
    gtk_gl_area_queue_render(GTK_GL_AREA(state->glArea));
}

// ─── Background load completion (main thread) ─────────────────────────────────

static void on_load_done(GObject *, GAsyncResult *res, gpointer user_data) {
    AppState *state = static_cast<AppState *>(user_data);

    GError *err = nullptr;
    LoadResult *result = static_cast<LoadResult *>(
        g_task_propagate_pointer(G_TASK(res), &err));

    // Hide spinner regardless of outcome.
    if (state->spinner)
        gtk_widget_set_visible(state->spinner, FALSE);

    if (err || !result) {
        fprintf(stderr, "orender-wire: error: failed to open '%s'\n", state->ribPath);
        if (err) g_error_free(err);
        // Leave blank window; user can close.
        return;
    }

    int w = gtk_widget_get_width(state->glArea);
    int h = gtk_widget_get_height(state->glArea);
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    if (result->isData) {
        state->dataDoc = result->dataDoc;
        const DataSceneC *snap = ribdata_snapshot(state->dataDoc);
        state->arcball = new ArcballCamera(
            snap->camera.projMatrix, snap->camera.viewMatrix,
            snap->bounds.sceneBoundsMin, snap->bounds.sceneBoundsMax,
            (float)w, (float)h);
        upload_data_scene(state, snap);
    } else {
        state->scene = result->ribScene;
        const PreviewCameraC &cam  = state->scene->camera;
        const PreviewBoundsC &bnds = state->scene->bounds;
        state->arcball = new ArcballCamera(
            cam.projMatrix, cam.viewMatrix,
            bnds.sceneBoundsMin, bnds.sceneBoundsMax,
            (float)w, (float)h);
        upload_scene(state);
    }

    delete result;   // ribScene/dataDoc ownership already transferred into state above

    update_header_bar_state(state);
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

static void trigger_save_camera(AppState *state) {
    if (!state->arcball) return;
    GtkWindow *win = GTK_WINDOW(gtk_widget_get_ancestor(state->glArea, GTK_TYPE_WINDOW));
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Export Camera");
    gtk_file_dialog_set_initial_name(dlg, "camera.rib");
    gtk_file_dialog_save(dlg, win, nullptr, save_camera_callback, state);
    g_object_unref(dlg);
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
        trigger_save_camera(state);
        return TRUE;
    case GDK_KEY_m:
    case GDK_KEY_M:
        apply_data_key(state, 'm');
        return TRUE;
    case GDK_KEY_l:
    case GDK_KEY_L:
        apply_data_key(state, 'l');
        return TRUE;
    case GDK_KEY_b:
    case GDK_KEY_B:
        apply_data_key(state, 'b');
        return TRUE;
    case GDK_KEY_d:
    case GDK_KEY_D:
        apply_data_key(state, 'd');
        return TRUE;
    case GDK_KEY_p:
    case GDK_KEY_P:
        apply_data_key(state, 'p');
        return TRUE;
    case GDK_KEY_w:
    case GDK_KEY_W:
        apply_data_key(state, 'w');
        return TRUE;
    case GDK_KEY_q:
    case GDK_KEY_Q: {
        // Legacy oshow convention: 'q' means "previous channel" for a document that has
        // channels. That collides with this app's own pre-existing "bare q/Q quits" binding, so
        // route by document state instead of always quitting -- Escape remains an unconditional
        // quit either way.
        bool hasChannels = state->dataDoc && ribdata_snapshot(state->dataDoc)->numChannels > 0;
        if (hasChannels) {
            apply_data_key(state, 'q');
            return TRUE;
        }
        [[fallthrough]];
    }
    case GDK_KEY_Escape: {
        GtkWindow *win = GTK_WINDOW(
            gtk_widget_get_ancestor(state->glArea, GTK_TYPE_WINDOW));
        if (win) gtk_window_close(win);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

// ─── Header-bar menu actions (User Story 2) ────────────────────────────────────
// Each forwards to the same apply_data_key()/trigger_save_camera() the keyboard uses -- one
// action implementation, two entry points, per T060.

static void action_prev_channel(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'q');
}
static void action_next_channel(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'w');
}
static void action_inc_detail(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'm');
}
static void action_dec_detail(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'l');
}
static void action_draw_boxes(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'b');
}
static void action_draw_discs(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'd');
}
static void action_draw_points(GSimpleAction *, GVariant *, gpointer user_data) {
    apply_data_key(static_cast<AppState *>(user_data), 'p');
}
static void action_save_camera(GSimpleAction *, GVariant *, gpointer user_data) {
    trigger_save_camera(static_cast<AppState *>(user_data));
}

// ─── Window close (clean shutdown) ───────────────────────────────────────────

static gboolean on_close_request(GtkWindow *, AppState *state) {
    if (state->scene) {
        ribpreview_free(state->scene);
        state->scene = nullptr;
    }
    if (state->dataDoc) {
        ribdata_close(state->dataDoc);
        state->dataDoc = nullptr;
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

    // AdwWindowTitle: shown in place of the plain title, subtitle carries document type /
    // channel / detail level / draw mode (FR-016) once a document loads -- see
    // update_header_bar_state(). gtk_window_set_title above still sets the taskbar/switcher
    // entry; this is purely the header-bar's own visual title.
    AdwWindowTitle *windowTitleWidget = ADW_WINDOW_TITLE(adw_window_title_new(title.c_str(), ""));
    state->windowTitle = windowTitleWidget;
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header_bar), GTK_WIDGET(windowTitleWidget));

    // Header-bar menu (User Story 2): one GSimpleAction per legacy key, added to the window's
    // own action group (invoked as "win.<name>" from the menu model below) and shared with the
    // existing on_key handler via apply_data_key()/trigger_save_camera() -- one action
    // implementation, two entry points. Enabled state is set per-document-type once a document
    // loads (update_header_bar_state()); until then everything starts disabled.
    auto addAction = [&](const char *name, GCallback cb) {
        GSimpleAction *act = g_simple_action_new(name, nullptr);
        g_simple_action_set_enabled(act, FALSE);
        g_signal_connect(act, "activate", cb, state);
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act));
        return act;
    };
    state->actPrevChannel = addAction("prev-channel", G_CALLBACK(action_prev_channel));
    state->actNextChannel = addAction("next-channel", G_CALLBACK(action_next_channel));
    state->actIncDetail   = addAction("inc-detail",   G_CALLBACK(action_inc_detail));
    state->actDecDetail   = addAction("dec-detail",   G_CALLBACK(action_dec_detail));
    state->actDrawBoxes   = addAction("draw-boxes",   G_CALLBACK(action_draw_boxes));
    state->actDrawDiscs   = addAction("draw-discs",   G_CALLBACK(action_draw_discs));
    state->actDrawPoints  = addAction("draw-points",  G_CALLBACK(action_draw_points));
    state->actSaveCamera  = addAction("save-camera",  G_CALLBACK(action_save_camera));
    // Save Camera works for any loaded document (RIB or data) until a data document is
    // specifically open -- start enabled, update_header_bar_state() disables it once needed.
    g_simple_action_set_enabled(state->actSaveCamera, TRUE);

    GMenu *menu = g_menu_new();

    GMenu *channelSection = g_menu_new();
    g_menu_append(channelSection, "Previous Channel", "win.prev-channel");
    g_menu_append(channelSection, "Next Channel", "win.next-channel");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(channelSection));
    g_object_unref(channelSection);

    GMenu *detailSection = g_menu_new();
    g_menu_append(detailSection, "Increase Detail Level", "win.inc-detail");
    g_menu_append(detailSection, "Decrease Detail Level", "win.dec-detail");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(detailSection));
    g_object_unref(detailSection);

    GMenu *drawSection = g_menu_new();
    g_menu_append(drawSection, "Draw as Boxes", "win.draw-boxes");
    g_menu_append(drawSection, "Draw as Discs", "win.draw-discs");
    g_menu_append(drawSection, "Draw as Points", "win.draw-points");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(drawSection));
    g_object_unref(drawSection);

    GMenu *cameraSection = g_menu_new();
    g_menu_append(cameraSection, "Save Camera\xE2\x80\xA6", "win.save-camera"); // U+2026 ELLIPSIS
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(cameraSection));
    g_object_unref(cameraSection);

    GtkWidget *menuButton = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menuButton), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menuButton), G_MENU_MODEL(menu));
    g_object_unref(menu);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), menuButton);

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

int main(int argc, char **argv) {
    // Suppress Mesa/EGL warnings and driver loader errors by forcing the OpenGL
    // renderer and silencing Mesa's internal diagnostic/error logging.
    setenv("GSK_RENDERER", "gl", 0);
    setenv("EGL_LOG_LEVEL", "fatal", 0);
    setenv("MESA_DEBUG", "silent", 0);
    setenv("LIBGL_DEBUG", "quiet", 0);
    setenv("MESA_LOG_FILE", "/dev/null", 0);

    // Delegates entirely to wireCliRun() (src/preview/libribpreview/wireCli.cpp) -- the shared,
    // already-tested (test_wire_cli.cpp) argument grammar, --help/--version text, --json
    // headless mode, and exit codes 1-4, so this frontend and the macOS one never drift from
    // each other or from contracts/cli-interface.md. This replaces main()'s own former ad-hoc
    // --help/--version/argc/access() checks, which never supported --json or --type at all.
    //
    // Must run, and return exitCode on WIRE_CLI_EXIT, before daemon()/GTK init below --
    // otherwise `--json` would daemonize into a background process before ever printing
    // anything, and a test harness (or an SSH session with no window server) would see exit 0
    // with no output instead of the JSON/text on this process's own stdout.
    char *outPath = nullptr;
    int wireExitCode = 0;
    WireCliAction wireAction = wireCliRun(argc, argv, &outPath, &wireExitCode);
    if (wireAction == WIRE_CLI_EXIT)
        return wireExitCode;

    // outPath is intentionally never freed -- AppState::ribPath keeps using this raw pointer
    // for the whole process lifetime (until exit), same as every other AppState member that
    // lives until on_close_request/process exit.
    const char *ribPath = outPath;

    // Release the terminal. nochdir=1 to keep CWD for RIB resources,
    // noclose=1 to keep stderr open for warnings.
    //
    // ORENDER_WIRE_GUI (same sentinel name as the macOS frontend's re-exec bypass) skips the
    // daemonize step for debugging: a crash after daemon() forks into the background is
    // otherwise invisible to the calling shell -- it detaches into a session the shell isn't
    // waiting on, so "no window, no error" is exactly what a post-fork crash looks like from
    // the terminal.
    if (getenv("ORENDER_WIRE_GUI") == nullptr) {
        if (daemon(1, 1) != 0) {
            perror("orender-wire: daemon failed");
            return 5;
        }
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
