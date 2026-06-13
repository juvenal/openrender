#include "activeContext.h"

namespace libshader {
thread_local CShadingContext *g_activeCtx = nullptr;
}
