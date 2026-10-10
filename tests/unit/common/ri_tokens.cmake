# Spec 023: token-name extraction shared by vocabulary_consistency.cmake and
# state_ownership.cmake.
#
# ri_tokens_declared(<file> <out-var>)
#   Every RI_* name declared in an `EXTERN(RtToken) ... ;` group (ri.h) or an
#   `extern RtToken ... ;` group (common/riVocabulary.h). Macros such as
#   RI_NULL or RIE_* constants are not tokens and are not matched.
# ri_tokens_defined(<file> <out-var>)
#   Every RI_* name defined as `RtToken RI_X = "...";` (common/riTokens.cpp).

function(ri_tokens_declared file out)
    file(READ "${file}" text)
    # Drop comments so commented-out names do not count.
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" text "${text}")
    string(REGEX REPLACE "//[^\n]*" "" text "${text}")
    string(REGEX MATCHALL "(EXTERN\\(RtToken\\)|extern[ \t]+RtToken)[^;]*;" groups "${text}")
    set(names "")
    foreach(group IN LISTS groups)
        string(REGEX REPLACE "^(EXTERN\\(RtToken\\)|extern[ \t]+RtToken)" "" group "${group}")
        string(REGEX MATCHALL "RI_[A-Z0-9_]+" found "${group}")
        list(APPEND names ${found})
    endforeach()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()

function(ri_tokens_defined file out)
    file(READ "${file}" text)
    string(REGEX REPLACE "//[^\n]*" "" text "${text}")
    string(REGEX MATCHALL "RtToken[ \t]+RI_[A-Z0-9_]+[ \t]*=" defs "${text}")
    set(names "")
    foreach(def IN LISTS defs)
        string(REGEX MATCH "RI_[A-Z0-9_]+" name "${def}")
        list(APPEND names "${name}")
    endforeach()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()
