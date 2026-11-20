# Sources moved to src/libshader/runtime/ and then (spec 033) into libRSLInfo / libRSLInfo_static (src/libshader/metadata/).
# These ALIAS targets preserve backward compatibility for anything that links "rslo": aliased to the real targets directly
# (an alias of an alias is not relied on).
add_library(rslo        ALIAS libRSLInfo)
add_library(rslo_static ALIAS libRSLInfo_static)
