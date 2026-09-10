# Compile the six native game module libraries (cgame/qagame/ui for baseq3
# and missionpack) from the sibling trinity checkout.
#
# trinity's build/srcs.mk is the single source manifest; its basenames are
# resolved to files using the same directory conventions as trinity's own
# build/Makefile.build pattern rules. The only deltas applied for native
# compilation are:
#   - each $(DIR)/*_syscalls.asm entry becomes the sibling *_syscalls.c
#   - the three missionpack targets compile with MISSIONPACK defined
# (bg_lib is NOT dropped: trinity's bg_lib.c guards the QVM libc replacement
# behind Q3_VM and its unguarded remainder is utility code - Q_vsprintf,
# BG_sprintf, Q_sscanf, BG_CleanName, ... - that the modules need natively.)
#
# A manifest entry that resolves to no file on disk is a configure error:
# drift between srcs.mk and the checkout must be loud.

if(NOT BUILD_GAME_LIBRARIES)
    return()
endif()

include(utils/set_output_dirs)

set(TRINITY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../trinity" CACHE PATH
    "Path to the trinity mod checkout the game module libraries are built from")

if(NOT IS_DIRECTORY "${TRINITY_SOURCE_DIR}")
    message(FATAL_ERROR
        "BUILD_GAME_LIBRARIES=ON but TRINITY_SOURCE_DIR is not a directory: "
        "${TRINITY_SOURCE_DIR}\nPoint -DTRINITY_SOURCE_DIR at a trinity checkout, "
        "or configure with -DBUILD_GAME_LIBRARIES=OFF for an engine-only build.")
endif()

set(TRINITY_SRCS_MK "${TRINITY_SOURCE_DIR}/build/srcs.mk")
if(NOT EXISTS "${TRINITY_SRCS_MK}")
    message(FATAL_ERROR
        "BUILD_GAME_LIBRARIES=ON but ${TRINITY_SRCS_MK} does not exist - "
        "TRINITY_SOURCE_DIR does not look like a trinity checkout.")
endif()

# Reconfigure whenever the manifest changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TRINITY_SRCS_MK}")

set(TRINITY_CODE_DIR  "${TRINITY_SOURCE_DIR}/code")
set(TRINITY_GAME_DIR  "${TRINITY_CODE_DIR}/game")
set(TRINITY_CGAME_DIR "${TRINITY_CODE_DIR}/cgame")
set(TRINITY_Q3UI_DIR  "${TRINITY_CODE_DIR}/q3_ui")
set(TRINITY_UI_DIR    "${TRINITY_CODE_DIR}/ui")

# Parse srcs.mk: three variables (QA_SRC, CG_SRC, UI_SRC) holding flat
# basename lists with backslash continuations plus $(DIR)/x_syscalls.asm
# entries, where CG_SRC and UI_SRC each appear in both arms of a single
# ifeq ($(CONFIG),missionpack)/else/endif conditional. Exactly that shape;
# anything else is a configure error.
function(trinity_parse_manifest MANIFEST)
    file(READ "${MANIFEST}" CONTENT)
    string(REPLACE "\r\n" "\n" CONTENT "${CONTENT}")
    # Make line continuation: backslash-newline splices lines together.
    string(REPLACE "\\\n" " " CONTENT "${CONTENT}")
    string(REPLACE ";" "\\;" CONTENT "${CONTENT}")
    string(REPLACE "\n" ";" MANIFEST_LINES "${CONTENT}")

    set(ARM shared) # shared | missionpack | baseq3
    foreach(VAR QA_SRC CG_SRC UI_SRC)
        foreach(SUFFIX shared missionpack baseq3)
            set(TOKENS_${VAR}_${SUFFIX} "")
        endforeach()
    endforeach()

    foreach(LINE IN LISTS MANIFEST_LINES)
        string(STRIP "${LINE}" LINE)

        if(LINE STREQUAL "" OR LINE MATCHES "^#")
            continue()
        elseif(LINE MATCHES "^ifeq")
            if(NOT LINE MATCHES "^ifeq[ \t]*\\(\\$\\(CONFIG\\),missionpack\\)$")
                message(FATAL_ERROR
                    "${MANIFEST}: unrecognized conditional '${LINE}' - this "
                    "parser only understands ifeq (\$(CONFIG),missionpack).")
            endif()
            set(ARM missionpack)
        elseif(LINE STREQUAL "else")
            set(ARM baseq3)
        elseif(LINE STREQUAL "endif")
            set(ARM shared)
        elseif(LINE MATCHES "^(QA_SRC|CG_SRC|UI_SRC)[ \t]*=[ \t]*(.*)$")
            set(VAR "${CMAKE_MATCH_1}")
            string(REGEX MATCHALL "[^ \t]+" TOKENS "${CMAKE_MATCH_2}")
            list(APPEND TOKENS_${VAR}_${ARM} ${TOKENS})
        else()
            message(FATAL_ERROR
                "${MANIFEST}: line not understood: '${LINE}' - the manifest "
                "shape changed; update cmake/trinity_modules.cmake.")
        endif()
    endforeach()

    set(TRINITY_QA_SRC             "${TOKENS_QA_SRC_shared}"      PARENT_SCOPE)
    set(TRINITY_CG_SRC_BASEQ3      "${TOKENS_CG_SRC_baseq3}"      PARENT_SCOPE)
    set(TRINITY_CG_SRC_MISSIONPACK "${TOKENS_CG_SRC_missionpack}" PARENT_SCOPE)
    set(TRINITY_UI_SRC_BASEQ3      "${TOKENS_UI_SRC_baseq3}"      PARENT_SCOPE)
    set(TRINITY_UI_SRC_MISSIONPACK "${TOKENS_UI_SRC_missionpack}" PARENT_SCOPE)
endfunction()

# Resolve manifest tokens to source files for one module, mirroring the
# pattern-rule search order in trinity's build/Makefile.build, and apply
# the native delta (*_syscalls.asm -> sibling *_syscalls.c).
#   OUT_VAR     - output list variable
#   MODULE_NAME - for error messages, e.g. "baseq3 cgame"
#   UIDIR       - what $(UIDIR) means for this module's config
#   SEARCH_DIRS - ;-list of directories tried in order for plain basenames
#   ARGN        - manifest tokens
function(trinity_resolve_sources OUT_VAR MODULE_NAME UIDIR SEARCH_DIRS)
    set(RESOLVED "")
    foreach(TOKEN IN LISTS ARGN)
        if(TOKEN MATCHES "^\\$\\((QADIR|CGDIR|UIDIR)\\)/([A-Za-z0-9_]+_syscalls)\\.asm$")
            if(CMAKE_MATCH_1 STREQUAL "QADIR")
                set(DIR "${TRINITY_GAME_DIR}")
            elseif(CMAKE_MATCH_1 STREQUAL "CGDIR")
                set(DIR "${TRINITY_CGAME_DIR}")
            else()
                set(DIR "${UIDIR}")
            endif()
            # Native delta: the QVM syscall stubs (.asm) become the DLL
            # syscall wrappers (.c) from the same module directory.
            set(FILE "${DIR}/${CMAKE_MATCH_2}.c")
            if(NOT EXISTS "${FILE}")
                message(FATAL_ERROR
                    "trinity manifest (${MODULE_NAME}): '${TOKEN}' has no "
                    "native sibling ${FILE}")
            endif()
            list(APPEND RESOLVED "${FILE}")
            continue()
        endif()
        if(TOKEN MATCHES "\\$")
            message(FATAL_ERROR
                "trinity manifest (${MODULE_NAME}): token '${TOKEN}' uses make "
                "syntax this parser does not understand.")
        endif()
        set(FOUND "")
        foreach(DIR IN LISTS SEARCH_DIRS)
            if(EXISTS "${DIR}/${TOKEN}.c")
                set(FOUND "${DIR}/${TOKEN}.c")
                break()
            endif()
        endforeach()
        if(FOUND STREQUAL "")
            message(FATAL_ERROR
                "trinity manifest (${MODULE_NAME}): '${TOKEN}' is listed in "
                "srcs.mk but ${TOKEN}.c was not found in any of: ${SEARCH_DIRS}")
        endif()
        list(APPEND RESOLVED "${FOUND}")
    endforeach()
    set(${OUT_VAR} "${RESOLVED}" PARENT_SCOPE)
endfunction()

trinity_parse_manifest("${TRINITY_SRCS_MK}")

foreach(LIST_NAME
        TRINITY_QA_SRC
        TRINITY_CG_SRC_BASEQ3 TRINITY_CG_SRC_MISSIONPACK
        TRINITY_UI_SRC_BASEQ3 TRINITY_UI_SRC_MISSIONPACK)
    if("${${LIST_NAME}}" STREQUAL "")
        message(FATAL_ERROR
            "${TRINITY_SRCS_MK}: parsing produced an empty ${LIST_NAME} - the "
            "manifest shape changed; update cmake/trinity_modules.cmake.")
    endif()
endforeach()

# Directory search order mirrors Makefile.build's pattern rules:
#   game modules:  $(QADIR)
#   cgame modules: $(QADIR), $(CGDIR), $(UIDIR)
#   ui modules:    $(QADIR), $(UIDIR)
# with $(UIDIR) = q3_ui for baseq3 and ui for missionpack.
trinity_resolve_sources(TRINITY_GAME_SOURCES "qagame"
    "" "${TRINITY_GAME_DIR}"
    ${TRINITY_QA_SRC})
trinity_resolve_sources(TRINITY_CGAME_SOURCES_BASEQ3 "baseq3 cgame"
    "${TRINITY_Q3UI_DIR}" "${TRINITY_GAME_DIR};${TRINITY_CGAME_DIR};${TRINITY_Q3UI_DIR}"
    ${TRINITY_CG_SRC_BASEQ3})
trinity_resolve_sources(TRINITY_CGAME_SOURCES_MISSIONPACK "missionpack cgame"
    "${TRINITY_UI_DIR}" "${TRINITY_GAME_DIR};${TRINITY_CGAME_DIR};${TRINITY_UI_DIR}"
    ${TRINITY_CG_SRC_MISSIONPACK})
trinity_resolve_sources(TRINITY_UI_SOURCES_BASEQ3 "baseq3 ui"
    "${TRINITY_Q3UI_DIR}" "${TRINITY_GAME_DIR};${TRINITY_Q3UI_DIR}"
    ${TRINITY_UI_SRC_BASEQ3})
trinity_resolve_sources(TRINITY_UI_SOURCES_MISSIONPACK "missionpack ui"
    "${TRINITY_UI_DIR}" "${TRINITY_GAME_DIR};${TRINITY_UI_DIR}"
    ${TRINITY_UI_SRC_MISSIONPACK})

# trinity's Makefile.build compiles with -DTRINITY_VERSION="git describe";
# ui_main.c/ui_menu.c/cg_servercmds.c consume it unconditionally.
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY "${TRINITY_SOURCE_DIR}"
    OUTPUT_VARIABLE TRINITY_MOD_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE TRINITY_MOD_VERSION_RESULT)
if(NOT TRINITY_MOD_VERSION_RESULT EQUAL 0 OR NOT TRINITY_MOD_VERSION)
    set(TRINITY_MOD_VERSION "unknown")
endif()

# The engine's version definitions, plus trinity's own version stamp.
list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
if(PROJECT_VERSION_MINOR STREQUAL "")
    list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION_MINOR=0)
else()
    list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION_MINOR=${PROJECT_VERSION_MINOR})
endif()
if(PROJECT_VERSION_PATCH STREQUAL "")
    list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION_PATCH=0)
else()
    list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION_PATCH=${PROJECT_VERSION_PATCH})
endif()
list(APPEND VERSION_DEFINITIONS TRINITY_VR_VERSION="${TRINITY_VR_VERSION_STRING}")
list(APPEND VERSION_DEFINITIONS TRINITY_VERSION="${TRINITY_MOD_VERSION}")

# Output names and output directories produce exactly the libraries
# Sys_LoadGameDll searches for.
set(MISSIONPACK "missionpack")

set(CGAME_MODULE_BINARY ${CGAME_MODULE})
set(GAME_MODULE_BINARY ${GAME_MODULE})
set(UI_MODULE_BINARY ${UI_MODULE})

set(CGAME_MODULE_BINARY_BASEGAME ${CGAME_MODULE_BINARY}_${BASEGAME})
set(GAME_MODULE_BINARY_BASEGAME ${GAME_MODULE_BINARY}_${BASEGAME})
set(UI_MODULE_BINARY_BASEGAME ${UI_MODULE_BINARY}_${BASEGAME})

set(CGAME_MODULE_BINARY_MISSIONPACK ${CGAME_MODULE_BINARY}_${MISSIONPACK})
set(GAME_MODULE_BINARY_MISSIONPACK ${GAME_MODULE_BINARY}_${MISSIONPACK})
set(UI_MODULE_BINARY_MISSIONPACK ${UI_MODULE_BINARY}_${MISSIONPACK})

# Include directories mirror trinity's own compile flags: -I$(QADIR) always
# (Q3LCC), plus -I$(CGDIR) [-I$(UIDIR) for missionpack] on cgame and
# -I$(UIDIR) on ui (config-*.mk).

add_library(                ${CGAME_MODULE_BINARY_BASEGAME} SHARED ${TRINITY_CGAME_SOURCES_BASEQ3})
target_compile_definitions( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE CGAME ${VERSION_DEFINITIONS})
target_include_directories( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${TRINITY_GAME_DIR} ${TRINITY_CGAME_DIR})
target_link_libraries(      ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${CGAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${CGAME_MODULE_BINARY})
set_output_dirs(            ${CGAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

add_library(                ${GAME_MODULE_BINARY_BASEGAME} SHARED ${TRINITY_GAME_SOURCES})
target_compile_definitions( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE QAGAME ${VERSION_DEFINITIONS})
target_include_directories( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${TRINITY_GAME_DIR})
target_link_libraries(      ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${GAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${GAME_MODULE_BINARY})
set_output_dirs(            ${GAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

add_library(                ${UI_MODULE_BINARY_BASEGAME} SHARED ${TRINITY_UI_SOURCES_BASEQ3})
target_compile_definitions( ${UI_MODULE_BINARY_BASEGAME} PRIVATE UI ${VERSION_DEFINITIONS})
target_include_directories( ${UI_MODULE_BINARY_BASEGAME} PRIVATE ${TRINITY_GAME_DIR} ${TRINITY_Q3UI_DIR})
target_link_libraries(      ${UI_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${UI_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${UI_MODULE_BINARY})
set_output_dirs(            ${UI_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

add_library(                ${CGAME_MODULE_BINARY_MISSIONPACK} SHARED ${TRINITY_CGAME_SOURCES_MISSIONPACK})
target_compile_definitions( ${CGAME_MODULE_BINARY_MISSIONPACK} PRIVATE CGAME MISSIONPACK ${VERSION_DEFINITIONS})
target_include_directories( ${CGAME_MODULE_BINARY_MISSIONPACK} PRIVATE ${TRINITY_GAME_DIR} ${TRINITY_CGAME_DIR} ${TRINITY_UI_DIR})
target_link_libraries(      ${CGAME_MODULE_BINARY_MISSIONPACK} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${CGAME_MODULE_BINARY_MISSIONPACK} PROPERTIES OUTPUT_NAME ${CGAME_MODULE_BINARY})
set_output_dirs(            ${CGAME_MODULE_BINARY_MISSIONPACK} SUBDIRECTORY ${MISSIONPACK})

add_library(                ${GAME_MODULE_BINARY_MISSIONPACK} SHARED ${TRINITY_GAME_SOURCES})
target_compile_definitions( ${GAME_MODULE_BINARY_MISSIONPACK} PRIVATE QAGAME MISSIONPACK ${VERSION_DEFINITIONS})
target_include_directories( ${GAME_MODULE_BINARY_MISSIONPACK} PRIVATE ${TRINITY_GAME_DIR})
target_link_libraries(      ${GAME_MODULE_BINARY_MISSIONPACK} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${GAME_MODULE_BINARY_MISSIONPACK} PROPERTIES OUTPUT_NAME ${GAME_MODULE_BINARY})
set_output_dirs(            ${GAME_MODULE_BINARY_MISSIONPACK} SUBDIRECTORY ${MISSIONPACK})

add_library(                ${UI_MODULE_BINARY_MISSIONPACK} SHARED ${TRINITY_UI_SOURCES_MISSIONPACK})
target_compile_definitions( ${UI_MODULE_BINARY_MISSIONPACK} PRIVATE UI MISSIONPACK ${VERSION_DEFINITIONS})
target_include_directories( ${UI_MODULE_BINARY_MISSIONPACK} PRIVATE ${TRINITY_GAME_DIR} ${TRINITY_UI_DIR})
target_link_libraries(      ${UI_MODULE_BINARY_MISSIONPACK} PRIVATE ${COMMON_LIBRARIES})
set_target_properties(      ${UI_MODULE_BINARY_MISSIONPACK} PROPERTIES OUTPUT_NAME ${UI_MODULE_BINARY})
set_output_dirs(            ${UI_MODULE_BINARY_MISSIONPACK} SUBDIRECTORY ${MISSIONPACK})
