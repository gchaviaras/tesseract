# Mac App Store .pkg. Included from Installers.cmake (APPLE only). The app
# bundle itself is signed by ui/macos/CMakeLists.txt; this adds the installer
# package target, defined only when both identities and the profile are set.
# CMake stops at a signed .pkg: validation and upload happen in CI.
if(TESSERACT_MAC_APP_STORE AND TESSERACT_MAC_APPSTORE_APP_IDENTITY
   AND TESSERACT_MAC_APPSTORE_INSTALLER_IDENTITY AND TESSERACT_MAC_PROVISIONING_PROFILE
   AND TESSERACT_MAC_TEAM_ID AND TARGET tesseract_macos)
    set(_appstore_pkg "${CMAKE_BINARY_DIR}/Tesseract-${PROJECT_VERSION}-appstore.pkg")
    add_custom_target(appstore-pkg
        DEPENDS tesseract_macos
        COMMAND ${CMAKE_COMMAND} -E remove -f "${_appstore_pkg}"
        COMMAND productbuild --component "$<TARGET_BUNDLE_DIR:tesseract_macos>" /Applications
                --sign "${TESSERACT_MAC_APPSTORE_INSTALLER_IDENTITY}" "${_appstore_pkg}"
        COMMAND pkgutil --check-signature "${_appstore_pkg}"
        COMMENT "Building ${_appstore_pkg}"
        VERBATIM
    )
endif()
