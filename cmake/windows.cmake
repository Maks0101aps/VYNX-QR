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

