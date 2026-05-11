# sn_audit.cmake — called by the sn-audit custom target
# Usage: cmake -DSRC=<source_dir> -P sn_audit.cmake

if(NOT DEFINED SRC)
    message(FATAL_ERROR "SRC not defined")
endif()

set(OUTFILE "${SRC}/sn_audit.txt")

file(GLOB LIB_CPP   "${SRC}/lib/*.cpp")
file(GLOB LIB_HPP   "${SRC}/lib/*.hpp")
file(GLOB CLI_CPP   "${SRC}/cli/*.cpp")
file(GLOB CLI_HPP   "${SRC}/cli/*.hpp")
file(GLOB GUI_CPP   "${SRC}/gui/*.cpp")
file(GLOB GUI_HPP   "${SRC}/gui/*.h")

file(GLOB MD_FILES    "${SRC}/*.md")
file(GLOB CMAKE_FILES "${SRC}/cmake/*.cmake")
file(GLOB SH_FILES    "${SRC}/scripts/*.sh")

set(ALL_SRC
    ${LIB_CPP} ${LIB_HPP}
    ${CLI_CPP} ${CLI_HPP}
    ${GUI_CPP} ${GUI_HPP}
    ${MD_FILES}
    ${CMAKE_FILES}
    ${SH_FILES}
)

foreach(EXTRA
    "${SRC}/CMakeLists.txt"
)
    if(EXISTS "${EXTRA}")
        list(APPEND ALL_SRC "${EXTRA}")
    endif()
endforeach()

string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")
file(WRITE "${OUTFILE}" "--- Watching SN Audit: ${NOW} ---\n")

foreach(F ${ALL_SRC})
    get_filename_component(FNAME "${F}" NAME)
    file(STRINGS "${F}" LINES)
    # Use only the last SN match — avoids false hits from doc examples in .md files
    set(FOUND_SN "")
    foreach(LINE ${LINES})
        if(LINE MATCHES "^(//|#|<!--) SN: ([0-9]+)")
            set(FOUND_SN "${CMAKE_MATCH_2}")
        endif()
    endforeach()
    if(NOT FOUND_SN STREQUAL "")
        string(LENGTH "${FNAME}" FLEN)
        set(SPACES "")
        if(FLEN LESS 35)
            math(EXPR PADLEN "35 - ${FLEN}")
            string(REPEAT " " ${PADLEN} SPACES)
        endif()
        file(APPEND "${OUTFILE}" "${FNAME}${SPACES}${FOUND_SN}\n")
    endif()
endforeach()

message(STATUS "SN Audit written to ${OUTFILE}")

# SN: 00001
