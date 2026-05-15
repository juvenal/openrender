# - Look for GNU Bison, the parser generator
# Based off a news post from Andy Cedilnik at Kitware
# Defines the following:
#  BISON_EXECUTABLE - path to the bison executable
#  BISON_FILE - parse a file with bison

IF(NOT DEFINED BISON_PREFIX_OUTPUTS)
 SET(BISON_PREFIX_OUTPUTS FALSE)
ENDIF(NOT DEFINED BISON_PREFIX_OUTPUTS)

IF(APPLE)
  # macOS ships Bison 2.3 which predates %define api.pure (requires 2.6+).
  # If BISON_EXECUTABLE is unset or points to the system bison, override it
  # with Homebrew's version. Apple Silicon: /opt/homebrew/bin; Intel: /usr/local/bin.
  IF(NOT BISON_EXECUTABLE OR BISON_EXECUTABLE STREQUAL "/usr/bin/bison")
    MESSAGE(STATUS "Looking for bison (preferring Homebrew over system)")
    FIND_PROGRAM(_bison_homebrew bison
      HINTS /opt/homebrew/opt/bison/bin /opt/homebrew/bin
            /usr/local/opt/bison/bin   /usr/local/bin
      NO_DEFAULT_PATH)
    IF(_bison_homebrew)
      SET(BISON_EXECUTABLE "${_bison_homebrew}" CACHE PATH "Path to bison" FORCE)
    ELSEIF(NOT BISON_EXECUTABLE)
      FIND_PROGRAM(BISON_EXECUTABLE bison)
    ENDIF()
    UNSET(_bison_homebrew CACHE)
    IF(BISON_EXECUTABLE)
      MESSAGE(STATUS "Looking for bison -- ${BISON_EXECUTABLE}")
    ENDIF()
  ENDIF()
ELSEIF(NOT BISON_EXECUTABLE)
  MESSAGE(STATUS "Looking for bison")
  FIND_PROGRAM(BISON_EXECUTABLE bison)
  IF(BISON_EXECUTABLE)
    MESSAGE(STATUS "Looking for bison -- ${BISON_EXECUTABLE}")
  ENDIF()
ENDIF()

IF(BISON_EXECUTABLE)
 MACRO(BISON_FILE FILENAME OUTNAME PREFIX)
   GET_FILENAME_COMPONENT(PATH "${FILENAME}" PATH)
#   IF("${PATH}" STREQUAL "")
     SET(PATH_OPT "")
#   ELSE("${PATH}" STREQUAL "")
#     SET(PATH_OPT "/${PATH}")
#   ENDIF("${PATH}" STREQUAL "")
   GET_FILENAME_COMPONENT(HEAD "${FILENAME}" NAME_WE)
   IF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
     FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
   ENDIF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
   SET(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}/${OUTNAME}")
   SET(LEXDEP "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}/lex.${PREFIX}.cpp")
     ADD_CUSTOM_COMMAND(
       OUTPUT "${OUTFILE}"
       COMMAND "${BISON_EXECUTABLE}"
       ARGS "--name-prefix=${PREFIX}"
       "--output-file=${OUTFILE}"
       "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}"
       DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}" "${LEXDEP}")
     SET_SOURCE_FILES_PROPERTIES("${OUTFILE}" PROPERTIES GENERATED TRUE)
     SET(BISON_OUTPUT_FILE "${OUTFILE}")
 ENDMACRO(BISON_FILE) 
ENDIF(BISON_EXECUTABLE)

