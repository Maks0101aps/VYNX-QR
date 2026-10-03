# Turns the versioned resource template into a concrete .rc file.
#
# The icon path and version numbers have to be absolute and numeric before rc.exe
# sees them, which cannot be expressed in the template itself. Run with `cmake -P`.

foreach(required VYNX_RC_IN VYNX_RC_OUT VYNX_RC_ICON VYNX_RC_VERSION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} was not provided")
  endif()
endforeach()

if(NOT EXISTS "${VYNX_RC_IN}")
  message(FATAL_ERROR "Resource template not found: ${VYNX_RC_IN}")
endif()
if(NOT EXISTS "${VYNX_RC_ICON}")
  message(FATAL_ERROR "Icon not found: ${VYNX_RC_ICON}")
endif()

# PROJECT_VERSION splits into three numbers; rc.exe wants dotted decimals.
string(REPLACE "." ";" version_parts "${VYNX_RC_VERSION}")
list(LENGTH version_parts version_count)
if(version_count LESS 3)
  math(EXPR padding "${3 - version_count}")
  while(padding GREATER 0)
    list(APPEND version_parts 0)
    math(EXPR padding "${padding} - 1")
  endwhile()
endif()
list(GET version_parts 0 version_major)
list(GET version_parts 1 version_minor)
list(GET version_parts 2 version_patch)

file(READ "${VYNX_RC_IN}" template)
string(REPLACE "@VYNX_RC_ICON@" "${VYNX_RC_ICON}" template "${template}")
string(REPLACE "@VYNX_RC_VERSION@" "${VYNX_RC_VERSION}" template "${template}")
string(REPLACE "@VYNX_RC_VERSION_MAJOR@" "${version_major}" template "${template}")
string(REPLACE "@VYNX_RC_VERSION_MINOR@" "${version_minor}" template "${template}")
string(REPLACE "@VYNX_RC_VERSION_PATCH@" "${version_patch}" template "${template}")

file(WRITE "${VYNX_RC_OUT}" "${template}")