include(GNUInstallDirs)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(VYNX_LINUX_DOCS "${CMAKE_BINARY_DIR}/linux-docs")
set(VYNX_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
file(MAKE_DIRECTORY "${VYNX_LINUX_DOCS}" "${VYNX_PACKAGE_DIR}")
add_custom_target(license_payload
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/generate-rust-licenses.py"
          --target x86_64-unknown-linux-gnu
          --output "${VYNX_LINUX_DOCS}/licenses/rust"
          --cache "${CMAKE_BINARY_DIR}/license-tools"
  VERBATIM)

# Install an explicit inventory, never the build directory or Rust target tree.
install(TARGETS VYNX_QR RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/vynx-qr.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
foreach(size IN ITEMS 48 64 128 256 512)
  install(FILES "${CMAKE_SOURCE_DIR}/app/resources/icons/linux/${size}x${size}.png"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
          RENAME vynx-qr.png)
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
              "${CMAKE_SOURCE_DIR}/packaging/linux/README-licenses.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/vynx-qr")
install(FILES "${CMAKE_SOURCE_DIR}/licenses/qt/LGPL-3.0.txt"
              "${CMAKE_SOURCE_DIR}/licenses/qt/GPL-3.0.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/vynx-qr/licenses/qt")
install(DIRECTORY "${VYNX_LINUX_DOCS}/licenses/rust"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/vynx-qr/licenses")

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME vynx-qr)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast, private QR code generator")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_DIRECTORY "${VYNX_PACKAGE_DIR}")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "VYNX")
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Platform plugins are loaded at runtime, so shlibdeps cannot discover them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins, qt6-wayland")
include(CPack)
add_custom_target(deb
  COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake" -G DEB
  DEPENDS VYNX_QR license_payload
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  VERBATIM)
