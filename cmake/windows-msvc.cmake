# Pins the build to MSVC.
#
# The hosted Windows images put MinGW's g++ on PATH, often ahead of MSVC, and
# CMake picks the first C++ compiler it finds. A MinGW build cannot link against
# Qt's msvc2022_64 binaries, and the failure that produces is actively misleading:
# find_package(Qt6) simply reports that Qt cannot be found, with nothing about the
# compiler. Two different problems, one unhelpful message.
#
# Ninja requires an initialized MSVC developer environment (PATH, INCLUDE, LIB).
# Naming cl selects the compiler; it does not locate or initialize Visual Studio.
# Use an x64 developer prompt locally and msvc-dev-cmd in GitHub Actions.

if(NOT WIN32)
  return()
endif()

# Only take effect on a first configure, so a developer who deliberately picked a
# toolchain in the cache is not overridden.
# MSVC is required for more than Qt: the Rust static library is built for
# x86_64-pc-windows-msvc, and mixing it with MinGW objects cannot link. So this is
# pinned here rather than left to whatever the PATH happens to offer first.
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "cl")
endif()
if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "cl")
endif()
