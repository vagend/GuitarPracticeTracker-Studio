cmake_minimum_required(VERSION 3.22)

project(GuitarPracticeStudio VERSION 0.3.0)

# JUCE is downloaded into external/JUCE by the setup script (it is not kept in git).
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "Path to the JUCE source folder")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found in ${JUCE_DIR}. Run 'Build Studio (Mac).command' first.")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_gui_app(GuitarPracticeStudio
    PRODUCT_NAME "Guitar Practice Studio"
    COMPANY_NAME "vagend"
    BUNDLE_ID "com.vagend.guitarpracticestudio"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/web/icons/icon-512.png"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_CURL FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Guitar Practice Studio uses your audio input so you can play your guitar through amp and effect plugins."
    PLUGINHOST_AU TRUE)

target_sources(GuitarPracticeStudio
    PRIVATE
        Source/Main.cpp)

target_compile_definitions(GuitarPracticeStudio
    PRIVATE
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarPracticeStudio,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarPracticeStudio,JUCE_VERSION>")

target_link_libraries(GuitarPracticeStudio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Copy the practice app's web files (index.html, trainer.html, icons, ...) into the app.
if(APPLE)
    set(WEB_DEST "$<TARGET_BUNDLE_CONTENT_DIR:GuitarPracticeStudio>/Resources/web")
else()
    set(WEB_DEST "$<TARGET_FILE_DIR:GuitarPracticeStudio>/web")
endif()
add_custom_command(TARGET GuitarPracticeStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${WEB_DEST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/web" "${WEB_DEST}"
    COMMENT "Copying web files into the app")
