set(PROJECT_NAME trinityvr)
set(PROJECT_VERSION 1.0.0)

# Override version from CI tag (GITHUB_REF_NAME) or git tag
if(DEFINED ENV{GITHUB_REF_NAME} AND "$ENV{GITHUB_REF_NAME}" MATCHES "^v([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
    set(PROJECT_VERSION "${CMAKE_MATCH_1}")
    set(TRINITY_VR_VERSION_STRING "$ENV{GITHUB_REF_NAME}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git describe --tags --abbrev=0
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_TAG_RESULT)

    if(GIT_TAG_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v?([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
        set(PROJECT_VERSION "${CMAKE_MATCH_1}")
    endif()

    # Full version string from git describe (e.g. "v1.0.11" or "v1.0.11-3-gabcdef-dirty")
    execute_process(
        COMMAND git describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE TRINITY_VR_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_DESCRIBE_RESULT)

    if(NOT GIT_DESCRIBE_RESULT EQUAL 0 OR NOT TRINITY_VR_VERSION_STRING)
        set(TRINITY_VR_VERSION_STRING "unknown")
    endif()
else()
    set(TRINITY_VR_VERSION_STRING "unknown")
endif()

set(SERVER_NAME trinityvr-ded)
set(CLIENT_NAME trinityvr)

set(BASEGAME baseq3)

set(CGAME_MODULE cgame)
set(GAME_MODULE qagame)
set(UI_MODULE ui)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/quake3.ico)
# Secondary icon embedded in the exe (IDI_ICON2) as an opt-in for user-created
# shortcuts; the default application icon stays WINDOWS_ICON_PATH above.
set(WINDOWS_ICON2_PATH ${CMAKE_SOURCE_DIR}/misc/trinityvr.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/quake3_flat.icns)
set(MACOS_BUNDLE_ID org.ioquake.${CLIENT_NAME})

set(COPYRIGHT "QUAKE III ARENA Copyright © 1999-2000 id Software, Inc. All rights reserved.")

set(CONTACT_EMAIL "info@ioquake.org")
set(PROTOCOL_HANDLER_SCHEME quake3)
