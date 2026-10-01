install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/authui7
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/look-and-feel
    PATTERN "watermark.png" EXCLUDE
    PATTERN "previews" EXCLUDE)
