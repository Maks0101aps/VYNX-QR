# Builds the Rust engine and bridges it into the native window.
#
# The whole product is one `cmake --build`: this file runs Cargo itself, so there
# is no separate Rust build step to remember and no chance of linking against a
# stale static library.

cmake_minimum_required(VERSION 3.24)

project(VYNX_QR
  VERSION 1.0.0
  DESCRIPTION "Fast, private QR codes for Windows."
  HOMEPAGE_URL "https://github.com/Maks0101aps/VYNX-QR"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

# Rust's MSVC target links the release C runtime in every profile, including
# debug, and there is no supported way to ask for the debug one. A C++ program
# linking a Rust static library must therefore use the release runtime as well,
# or the linker rejects the mix. Qt is used from its release binaries for the
# same reason.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Single output directory, so the binary and its DLLs land together.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

option(VYNX_BUILD_TESTS "Build the bridge and engine tests" ON)

# Rust on the MSVC target links the release C runtime in every profile, and there
# is no supported way to ask for the debug one. A Qt debug build links the debug
# runtime, so the two cannot coexist in one process. The application is therefore
# always built Release; the debug preset is for the engine and the bridge test,
# neither of which pulls in Qt.
option(VYNX_BUILD_APP "Build the application (requires a release Qt)" ON)
option(VYNX_DEPLOY_QT "Copy the Qt runtime next to the binary with windeployqt" OFF)

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
# The window tests drive the real window, so they need Qt Widgets even in the
# debug preset, where the application itself is deliberately not built.
option(VYNX_BUILD_WINDOW_TESTS "Build the Qt window tests" ON)

if(VYNX_BUILD_APP OR VYNX_BUILD_WINDOW_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
endif()
if(VYNX_BUILD_WINDOW_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

if(VYNX_BUILD_APP AND Qt6_VERSION VERSION_LESS 6.5)
  message(FATAL_ERROR "Qt 6.5 or newer is required, found ${Qt6_VERSION}")
endif()

if(VYNX_BUILD_APP)
  qt_standard_project_setup()
endif()

# ---------------------------------------------------------------------------
# Rust engine and CXX bridge
# ---------------------------------------------------------------------------
find_program(CARGO_EXECUTABLE cargo
  HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{HOME}/.cargo/bin"
  REQUIRED
)

# Cargo writes its static library into the profile directory. Keep it inside the
# CMake binary directory so a clean removes both, and so this file knows where to
# look without depending on a globally configured CARGO_TARGET_DIR.
#
# Cargo reads this from the environment, not from a CMake variable, so it has to
# be passed through `cmake -E env` or it is silently ignored.
set(VYNX_CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
set(CARGO_TARGET_DIR "${VYNX_CARGO_TARGET_DIR}" CACHE PATH "Cargo output directory")

# The Rust library must use the same C runtime as the C++ side, or the linker
# refuses to mix MDd and MD objects. Debug CMake therefore builds a debug engine.
set(VYNX_CARGO_BUILD_FLAGS "")
set(VYNX_CARGO_PROFILE_SUBDIR "debug")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(VYNX_CARGO_BUILD_FLAGS "--release")
  set(VYNX_CARGO_PROFILE_SUBDIR "release")
endif()

set(VYNX_CARGO_PROFILE_DIR "${VYNX_CARGO_TARGET_DIR}/${VYNX_CARGO_PROFILE_SUBDIR}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${VYNX_CARGO_TARGET_DIR}"
          "${CARGO_EXECUTABLE}" build ${VYNX_CARGO_BUILD_FLAGS}
          --manifest-path "${CMAKE_SOURCE_DIR}/bridge/Cargo.toml"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE cargo_output
  ERROR_VARIABLE cargo_output
  RESULT_VARIABLE cargo_result
)
if(NOT cargo_result EQUAL 0)
  message(FATAL_ERROR "Building the Rust bridge failed:\n${cargo_output}")
endif()

# The C++ header cxx generates, which the window includes.
# The header cxx generates sits at <root>/vynx-qr-bridge/src/lib.rs.h, with
# rust/cxx.h beside it under the same root. That root is what goes on the include
# path, so sources include <vynx-qr-bridge/src/lib.rs.h>.
#
# Cargo nests it under a build directory whose name changes with the toolchain
# version, so find it by suffix and take the three levels above it.
file(GLOB_RECURSE VYNX_BRIDGE_HEADERS "${VYNX_CARGO_TARGET_DIR}/lib.rs.h")
set(VYNX_BRIDGE_HEADER "")
foreach(candidate IN LISTS VYNX_BRIDGE_HEADERS)
  if(candidate MATCHES "vynx-qr-bridge")
    set(VYNX_BRIDGE_HEADER "${candidate}")
    break()
  endif()
endforeach()
if(NOT VYNX_BRIDGE_HEADER)
  message(FATAL_ERROR
    "cxx generated no header under ${VYNX_CARGO_TARGET_DIR}. "
    "Check that bridge/src/lib.rs parses by running cargo build there.")
endif()

get_filename_component(VYNX_BRIDGE_INCLUDE_ROOT "${VYNX_BRIDGE_HEADER}" DIRECTORY)
get_filename_component(VYNX_BRIDGE_INCLUDE_ROOT "${VYNX_BRIDGE_INCLUDE_ROOT}" DIRECTORY)
get_filename_component(VYNX_BRIDGE_INCLUDE_ROOT "${VYNX_BRIDGE_INCLUDE_ROOT}" DIRECTORY)

message(STATUS "Bridge header: ${VYNX_BRIDGE_HEADER}")

set(VYNX_BRIDGE_LIB "${VYNX_CARGO_PROFILE_DIR}/vynx_qr_bridge.lib")

if(NOT EXISTS "${VYNX_BRIDGE_LIB}")
  message(FATAL_ERROR "Expected the Rust static library at ${VYNX_BRIDGE_LIB}")
endif()

message(STATUS "Rust bridge: ${VYNX_BRIDGE_LIB}")
message(STATUS "Bridge include root: ${VYNX_BRIDGE_INCLUDE_ROOT}")

# ---------------------------------------------------------------------------
# The window
# ---------------------------------------------------------------------------
if(VYNX_BUILD_APP)
add_executable(VYNX_QR WIN32
  app/main.cpp
  app/MainWindow.cpp
  app/MainWindow.h
  app/Theme.cpp
  app/Theme.h
  app/WindowsIntegration.cpp
  app/WindowsIntegration.h
  app/widgets/AboutDialog.cpp
  app/widgets/AboutDialog.h
  app/widgets/Bridge.cpp
  app/widgets/Bridge.h
  app/widgets/ClipboardSuggestion.cpp
  app/widgets/ClipboardSuggestion.h
  app/widgets/CustomizePanel.cpp
  app/widgets/CustomizePanel.h
  app/widgets/QrPreview.cpp
  app/widgets/QrPreview.h
  app/widgets/SettingsDialog.cpp
  app/widgets/SettingsDialog.h
  app/widgets/SpecialQrForms.cpp
  app/widgets/SpecialQrForms.h
)

target_include_directories(VYNX_QR PRIVATE
  "${CMAKE_SOURCE_DIR}/app"
  ${VYNX_BRIDGE_INCLUDE_ROOT}
)

target_link_libraries(VYNX_QR PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  "${VYNX_BRIDGE_LIB}"
)

target_compile_definitions(VYNX_QR PRIVATE
  VYNX_VERSION="${PROJECT_VERSION}"
  VYNX_REPOSITORY="${PROJECT_HOMEPAGE_URL}"
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
)

if(MSVC)
  target_compile_options(VYNX_QR PRIVATE /W4 /permissive- /utf-8 /wd4996)
  # A GUI subsystem binary must not open a console window in release.
  set_target_properties(VYNX_QR PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
  )
endif()

# ---------------------------------------------------------------------------
# Windows resources: icon, version information, common controls v6
# ---------------------------------------------------------------------------
set(VYNX_ICON_DIR "app/resources/icons")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VYNX_RC_ARCH "x64")
else()
  set(VYNX_RC_ARCH "x86")
endif()
set(VYNX_RC_ICON "${CMAKE_SOURCE_DIR}/${VYNX_ICON_DIR}/icon.ico")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vynx_qr.rc"
  COMMAND ${CMAKE_COMMAND} -DVYNX_RC_IN=${CMAKE_SOURCE_DIR}/app/resources/vynx_qr.rc.in
          -DVYNX_RC_OUT=${CMAKE_CURRENT_BINARY_DIR}/vynx_qr.rc
          -DVYNX_RC_ICON=${VYNX_RC_ICON}
          -DVYNX_RC_ARCH=${VYNX_RC_ARCH}
          -DVYNX_RC_VERSION=${PROJECT_VERSION}
          -P ${CMAKE_SOURCE_DIR}/cmake/ConfigureResource.cmake
  DEPENDS "${CMAKE_SOURCE_DIR}/app/resources/vynx_qr.rc.in"
          "${CMAKE_SOURCE_DIR}/cmake/ConfigureResource.cmake"
          "${VYNX_RC_ICON}"
  COMMENT "Configuring the Windows resource script"
  VERBATIM
)
target_sources(VYNX_QR PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/vynx_qr.rc")

# The executable is called "VYNX QR.exe", which is what Windows Search shows and
# what the installer registers. The target itself keeps an identifier CMake can
# safely reference.
set_target_properties(VYNX_QR PROPERTIES OUTPUT_NAME "VYNX QR")

# What goes into the installer: the executable and the Qt runtime that sits next
# to it. The runtime is deployed by the POST_BUILD step above, so it is installed
# from the same directory rather than copied twice.
install(TARGETS VYNX_QR RUNTIME DESTINATION ".")
install(DIRECTORY "$<TARGET_FILE_DIR:VYNX_QR>/"
  DESTINATION "."
  USE_SOURCE_PERMISSIONS
  PATTERN "Qt6*.dll"
  PATTERN "*.exe"
  PATTERN "platforms/*.dll"
  PATTERN "styles/*.dll"
)

endif() # VYNX_BUILD_APP

# ---------------------------------------------------------------------------
# Qt runtime deployment
# ---------------------------------------------------------------------------
if(VYNX_DEPLOY_QT AND VYNX_BUILD_APP)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt
  HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/bin"
)
  if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt was not found next to the Qt installation")
  endif()
  add_custom_command(TARGET VYNX_QR POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations --no-system-d3d-compiler --no-opengl-sw
            --no-quick-import --no-compiler-runtime
            --dir "$<TARGET_FILE_DIR:VYNX_QR>"
            "$<TARGET_FILE:VYNX_QR>"
    COMMENT "Copying the Qt runtime next to the executable"
    VERBATIM
  )

  # windeployqt copies for the whole of Qt, not for this application, and leaves
  # roughly 21 MB that this program can never reach. Nothing here loads an image
  # from disk through Qt, because logos are decoded by the Rust engine and handed
  # over as raw pixels; nothing reaches the network, because the vector export is
  # written as text and the clipboard is read through QtGui alone. So the shader
  # compiler, the network stack, the SVG image plugin and the plugin directories
  # that belong to them are removed, and the result is verified by the application
  # still starting.
  set(VYNX_DEPLOY_UNNEEDED
    dxcompiler.dll        # DirectX shader compiler, for RHI paths this app never takes
    dxil.dll
    Qt6Network.dll        # no sockets are opened by this application
    Qt6Svg.dll            # the vector export is text, written by the engine
    imageformats          # images reach Qt as decoded RGBA, never as files
    tls
    networkinformation
    iconengines
    generic
  )
  foreach(item IN LISTS VYNX_DEPLOY_UNNEEDED)
    add_custom_command(TARGET VYNX_QR POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "$<TARGET_FILE_DIR:VYNX_QR>/${item}"
      COMMENT "Removing ${item}, which this application never loads"
      VERBATIM
    )
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(VYNX_BUILD_TESTS)
  enable_testing()

  # A C++ test that calls the bridge for real. If this links and passes, the
  # engine, the generated header and the C++ side all agree.
  add_executable(vynx_bridge_test tests/bridge_test.cpp)
  target_include_directories(vynx_bridge_test PRIVATE ${VYNX_BRIDGE_INCLUDE_ROOT})
  target_link_libraries(vynx_bridge_test PRIVATE "${VYNX_BRIDGE_LIB}")
  if(MSVC)
    target_compile_options(vynx_bridge_test PRIVATE /W4 /permissive- /utf-8)
  endif()
  add_test(NAME bridge COMMAND vynx_bridge_test)

  # The window under test needs the application's own sources, so it is built from
  # the same list rather than duplicating it.
  add_executable(vynx_window_test
    tests/MainWindow_test.cpp
    app/MainWindow.cpp
    app/widgets/AboutDialog.cpp
    app/widgets/AboutDialog.h
    app/MainWindow.h
    app/Theme.cpp
    app/Theme.h
    app/WindowsIntegration.cpp
    app/WindowsIntegration.h
    app/widgets/Bridge.cpp
    app/widgets/Bridge.h
    app/widgets/ClipboardSuggestion.cpp
    app/widgets/ClipboardSuggestion.h
    app/widgets/CustomizePanel.cpp
    app/widgets/CustomizePanel.h
    app/widgets/QrPreview.cpp
    app/widgets/QrPreview.h
    app/widgets/SettingsDialog.cpp
    app/widgets/SettingsDialog.h
    app/widgets/SpecialQrForms.cpp
    app/widgets/SpecialQrForms.h
  )
  target_include_directories(vynx_window_test PRIVATE
    "${CMAKE_SOURCE_DIR}/app"
    ${VYNX_BRIDGE_INCLUDE_ROOT}
  )
  target_link_libraries(vynx_window_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    "${VYNX_BRIDGE_LIB}"
    ${VYNX_RUST_SYSTEM_LIBS}
  )
  target_compile_definitions(vynx_window_test PRIVATE
    VYNX_VERSION="${PROJECT_VERSION}"
    VYNX_REPOSITORY="${PROJECT_HOMEPAGE_URL}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
  )
  if(MSVC)
    target_compile_options(vynx_window_test PRIVATE /W4 /permissive- /utf-8 /wd4996)
  endif()
  add_test(NAME window COMMAND vynx_window_test)

  # Qt on PATH, because the test binary links the Qt import libraries and does
  # not carry the DLLs themselves.
  #
  # The test deliberately runs on the normal Windows platform rather than the
  # offscreen one. Several windows are created and destroyed in sequence here, and
  # the offscreen plugin crashes on that sequence with a stack buffer overrun while
  # the real platform passes; a test that only runs on one platform is worth less
  # than one that runs on the platform users actually have.
  # Qt6_DIR is <prefix>/lib/cmake/Qt6, so the DLLs are three levels up. The path is
  # checked here rather than discovered at test time, because a wrong one shows up
  # as a bare 0xc0000135 with nothing to explain it.
  get_filename_component(VYNX_QT_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
  if(NOT EXISTS "${VYNX_QT_BIN}")
    message(FATAL_ERROR "Qt DLL directory not found at ${VYNX_QT_BIN}")
  endif()
  # ENVIRONMENT_MODIFICATION rather than a hand built PATH: the system PATH is
  # full of semicolons, which CMake would read as list separators and mangle.
  set_tests_properties(window PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${VYNX_QT_BIN}"
  )
endif()

# ---------------------------------------------------------------------------
# Linking the Rust static library
# ---------------------------------------------------------------------------
# Rust's standard library references parts of the Windows API a C++ program would
# never otherwise touch, so a consumer of a Rust static library has to name them.
# This is the usual set for an MSVC consumer of rustc output. It is declared here
# rather than next to the application because both the window and the bridge test
# link it, and the test exists in configurations where the window does not.
set(VYNX_RUST_SYSTEM_LIBS
  ws2_32      # sockets, used by std::net
  ntdll       # native file operations
  userenv     # environment and profiles
  bcrypt      # hashing primitives
  crypt32     # certificate and crypto API
  advapi32    # registry and security
  shell32     # shell operations
  ole32       # COM runtime
  oleaut32    # automation
  uuid        # COM interface identifiers
  dwmapi      # window chrome, used by the native integration
)

if(VYNX_BUILD_APP)
  target_link_libraries(VYNX_QR PRIVATE ${VYNX_RUST_SYSTEM_LIBS})
endif()
if(VYNX_BUILD_TESTS)
  target_link_libraries(vynx_bridge_test PRIVATE ${VYNX_RUST_SYSTEM_LIBS})
endif()
if(VYNX_BUILD_WINDOW_TESTS)
  target_link_libraries(vynx_window_test PRIVATE ${VYNX_RUST_SYSTEM_LIBS})
endif()

# ---------------------------------------------------------------------------
# Installer
# ---------------------------------------------------------------------------
# Written by hand rather than generated by CPack, for two reasons that both show
# up immediately when it is used here: the Start Menu shortcut is generated from
# the CMake target name, so it points at "VYNX_QR.exe" rather than at
# "VYNX QR.exe", and CPack's generator still asks for an administrator, which is
# wrong for a per-user utility. This script is per-user, names the executable
# correctly, and registers an Apps and Features entry.

set(VYNX_PAYLOAD_DIR "${CMAKE_BINARY_DIR}/bin")
set(VYNX_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
file(MAKE_DIRECTORY "${VYNX_PACKAGE_DIR}")

# Point NSIS at the same payload directory the build produced.
configure_file("${CMAKE_SOURCE_DIR}/installer/vynx_qr.nsi" "${VYNX_PACKAGE_DIR}/vynx_qr.nsi" @ONLY)

find_program(MAKENSIS_EXECUTABLE makensis
  HINTS "$ENV{LOCALAPPDATA}/tauri/NSIS" "C:/Program Files (x86)/NSIS"
)
if(NOT MAKENSIS_EXECUTABLE)
  # Not fatal: the application and its tests build without an installer.
  message(STATUS "makensis was not found, so the installer target is unavailable")
else()
  add_custom_target(installer
    COMMAND "${MAKENSIS_EXECUTABLE}" "/DVYNX_VERSION=${PROJECT_VERSION}"
            "/DVYNX_PAYLOAD_DIR=${VYNX_PAYLOAD_DIR}"
            "/DVYNX_ICON=${VYNX_RC_ICON}"
            "/DVYNX_LICENSE=${CMAKE_SOURCE_DIR}/LICENSE"
            "/XSetCompressor /SOLID lzma"
            "/XOutFile ${VYNX_PACKAGE_DIR}/VYNX-QR-Setup-x64-${PROJECT_VERSION}.exe"
            "${VYNX_PACKAGE_DIR}/vynx_qr.nsi"
    DEPENDS VYNX_QR "${VYNX_PACKAGE_DIR}/vynx_qr.nsi"
    COMMENT "Building the NSIS installer"
    VERBATIM
  )
endif()

# ---------------------------------------------------------------------------
# Portable archive
# ---------------------------------------------------------------------------
# The same payload with nothing installed: useful to run from a USB stick, and it
# costs one archive step rather than a second packaging tool.
if(VYNX_BUILD_APP)
  add_custom_target(portable
    COMMAND "${CMAKE_COMMAND}" -E tar czf
            "${VYNX_PACKAGE_DIR}/VYNX-QR-Portable-x64-${PROJECT_VERSION}.zip"
            --format=zip "VYNX QR.exe" "Qt6Core.dll" "Qt6Gui.dll" "Qt6Widgets.dll"
            "platforms" "styles"
    WORKING_DIRECTORY "${VYNX_PAYLOAD_DIR}"
    DEPENDS VYNX_QR
    COMMENT "Building the portable archive"
    VERBATIM
  )
endif()

include(CPack)
