# Hiders_ReyesGridShadeRace (GitHub #127). Run as
#   cmake -DORENDER=<orender> -DDISPLAYDIR=<dir of the file driver> -DSHADERDIR=<dir> -DWORKDIR=<dir> -P this
# Only registered in an OPENRENDER_TSAN tree (tests/unit/raster/CMakeLists.txt).
#
# A REYES grid is queued once and drawn by every bucket it overlaps, on whichever thread takes the bucket.
# The first thread to need its colour shades it under the grid's mutex while another thread tests the grid's
# flags and reads its vertex data without any ordering. The race depends on thread timing, so this test
# cannot be exact: it renders several overlapping, multi-bucket grids on four threads with the reyes hider
# and the zbuffer hider under ThreadSanitizer (abort_on_error=0, so every report is logged) and fails when
# ANY report's memory is a REYES grid (its heap block is allocated by CReyes::newGrid). The unrelated races
# of other blocks (the CPatch and stats counters, GitHub #128) are not matched. A pass is evidence, not a
# proof; on the unfixed tree the same scene reports the grid on essentially every run.
if(NOT ORENDER OR NOT WORKDIR OR NOT DISPLAYDIR OR NOT SHADERDIR)
    message(FATAL_ERROR "Hiders_ReyesGridShadeRace: ORENDER, DISPLAYDIR, SHADERDIR and WORKDIR are required")
endif()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

set(_frames "")
foreach(_hider hidden zbuffer hidden zbuffer)
    string(APPEND _frames "
FrameBegin 1
    Format 192 192 1
    PixelSamples 2 2
    PixelFilter \"box\" 1 1
    Hider \"${_hider}\"
    Projection \"perspective\" \"fov\" 40
    Display \"grid_race.tif\" \"file\" \"rgba\"
    Translate 0 0 7
    WorldBegin
        Surface \"matte\"
        AttributeBegin
            Rotate 25 1 0 0
            Sphere 2 -2 2 360
        AttributeEnd
        AttributeBegin
            Translate 1.2 0.4 -1
            Rotate 60 0 1 0
            Cylinder 1.5 -2 2 360
        AttributeEnd
        AttributeBegin
            Translate -1.5 -0.5 0.5
            Sphere 1.5 -1.5 1.5 360
        AttributeEnd
        AttributeBegin
            Translate 0 0 -3
            Rotate -30 1 1 0
            Disk 0 3 360
        AttributeEnd
    WorldEnd
FrameEnd
")
endforeach()
file(WRITE "${WORKDIR}/grid_race.rib" "
Option \"shaderformat\" \"default\" [\"rslo\"]
Option \"searchpath\" \"shader\" [\"${SHADERDIR}\"]
Option \"searchpath\" \"display\" [\"${DISPLAYDIR}\"]
Option \"limits\" \"numthreads\" [4]
Option \"limits\" \"bucketsize\" [8 8]
${_frames}")

set(ENV{TSAN_OPTIONS} "abort_on_error=0:halt_on_error=0:exitcode=0:log_path=${WORKDIR}/tsan")
set(_failures 0)
set(_reports 0)
set(_grid_reports 0)
set(_first_grid_report "")

# Several runs, because the interleaving differs from run to run.
foreach(_run RANGE 1 4)
    execute_process(COMMAND "${ORENDER}" "${WORKDIR}/grid_race.rib"
                    WORKING_DIRECTORY "${WORKDIR}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(SEND_ERROR "run ${_run}: orender exited with '${_rc}'\n${_out}\n${_err}")
        math(EXPR _failures "${_failures} + 1")
    endif()
endforeach()
if(NOT EXISTS "${WORKDIR}/grid_race.tif")
    message(SEND_ERROR "orender produced no image: the scene did not render")
    math(EXPR _failures "${_failures} + 1")
endif()

file(GLOB _logs "${WORKDIR}/tsan.*")
foreach(_log ${_logs})
    file(READ "${_log}" _text)
    string(REPLACE "\n==================\nWARNING: ThreadSanitizer" "\n@@SPLIT@@WARNING: ThreadSanitizer" _text "${_text}")
    string(REPLACE ";" "\;" _text "${_text}")
    string(REGEX REPLACE "@@SPLIT@@" ";" _chunks "${_text}")
    foreach(_chunk ${_chunks})
        if(NOT _chunk MATCHES "WARNING: ThreadSanitizer")
            continue()
        endif()
        math(EXPR _reports "${_reports} + 1")
        if(_chunk MATCHES "heap block of size [0-9]+ at [0-9a-fx]+ allocated by thread [^\n]*\n[^\n]*\n[^\n]*CReyes::newGrid")
            math(EXPR _grid_reports "${_grid_reports} + 1")
            if(_first_grid_report STREQUAL "")
                set(_first_grid_report "${_chunk}")
            endif()
        endif()
    endforeach()
endforeach()

message(STATUS "ThreadSanitizer reports in 4 runs: ${_reports}; on a REYES grid: ${_grid_reports}")
if(_grid_reports GREATER 0)
    message(SEND_ERROR "${_grid_reports} data race report(s) on a REYES grid (CReyes::newGrid block). First:\n${_first_grid_report}")
    math(EXPR _failures "${_failures} + 1")
endif()
if(_failures GREATER 0)
    message(FATAL_ERROR "Hiders_ReyesGridShadeRace failed")
endif()
