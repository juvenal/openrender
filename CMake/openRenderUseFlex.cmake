# - Look for GNU flex, the lexer generator.
# Defines the following:
#  FLEX_EXECUTABLE - path to the flex executable
#  FLEX_FILE(infile outfile prefix) - parse a file with flex

IF(APPLE)
  # macOS ships an old system flex; keg-only Homebrew flex is not in the main bin dir.
  # Apple Silicon: /opt/homebrew/opt/flex/bin; Intel: /usr/local/opt/flex/bin.
  IF(NOT FLEX_EXECUTABLE OR FLEX_EXECUTABLE STREQUAL "/usr/bin/flex")
    MESSAGE(STATUS "Looking for flex (preferring Homebrew over system)")
    FIND_PROGRAM(_flex_homebrew flex
      HINTS /opt/homebrew/opt/flex/bin /opt/homebrew/bin
            /usr/local/opt/flex/bin    /usr/local/bin
      NO_DEFAULT_PATH)
    IF(_flex_homebrew)
      SET(FLEX_EXECUTABLE "${_flex_homebrew}" CACHE PATH "Path to flex" FORCE)
    ELSEIF(NOT FLEX_EXECUTABLE)
      FIND_PROGRAM(FLEX_EXECUTABLE flex)
    ENDIF()
    UNSET(_flex_homebrew CACHE)
    IF(FLEX_EXECUTABLE)
      MESSAGE(STATUS "Looking for flex -- ${FLEX_EXECUTABLE}")
    ENDIF()
  ENDIF()
ELSEIF(NOT FLEX_EXECUTABLE)
  MESSAGE(STATUS "Looking for flex")
  FIND_PROGRAM(FLEX_EXECUTABLE flex)
  IF(FLEX_EXECUTABLE)
    MESSAGE(STATUS "Looking for flex -- ${FLEX_EXECUTABLE}")
  ENDIF()
ENDIF()

IF(FLEX_EXECUTABLE)
  MACRO(FLEX_FILE FILENAME OUTNAME PREFIX)
    GET_FILENAME_COMPONENT(PATH "${FILENAME}" PATH)
#    IF("${PATH}" STREQUAL "")
      SET(PATH_OPT "")
#    ELSE("${PATH}" STREQUAL "")
#      SET(PATH_OPT "/${PATH}")
#    ENDIF("${PATH}" STREQUAL "")
    IF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
      FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
    ENDIF(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}")
    SET(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}${PATH_OPT}/${OUTNAME}")
    ADD_CUSTOM_COMMAND(
      OUTPUT "${OUTFILE}"
      COMMAND "${FLEX_EXECUTABLE}"
      ARGS "--prefix=${PREFIX}"
      "--outfile=${OUTFILE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}")
    SET_SOURCE_FILES_PROPERTIES("${OUTFILE}" PROPERTIES GENERATED TRUE)
  ENDMACRO(FLEX_FILE)
ENDIF(FLEX_EXECUTABLE)
