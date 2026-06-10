# Sources moved to src/libshader/runtime/ (libshader_runtime / libshader_runtime_static).
# These ALIAS targets preserve backward compatibility for anything that links "rslo".
add_library(rslo        ALIAS libshader_runtime)
add_library(rslo_static ALIAS libshader_runtime_static)
