cmake_minimum_required(VERSION 3.22)
project(flashcard_quiz VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# Windows: link the C runtime statically so classmates need no VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE lookup order (download is pinned to the version this was tested on): own .juce_sdk, sibling midi_repinator checkout, then download.
if(EXISTS "${CMAKE_SOURCE_DIR}/.juce_sdk/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${CMAKE_SOURCE_DIR}/.juce_sdk")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../midi_repinator/.juce_sdk/CMakeLists.txt")
    FetchContent_Declare(JUCE SOURCE_DIR "${CMAKE_SOURCE_DIR}/../midi_repinator/.juce_sdk")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
endif()
FetchContent_MakeAvailable(JUCE)

# Engine: deck model, CSV import, JSON save, Leitner scheduler, EPUB export. No GUI.
add_library(flashcard_engine STATIC
    src/Deck.cpp
    src/Scheduler.cpp
    src/Epub.cpp)
target_include_directories(flashcard_engine PUBLIC src)
target_compile_definitions(flashcard_engine PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(flashcard_engine PUBLIC
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Console test runner for the engine.
juce_add_console_app(flashcard_engine_test PRODUCT_NAME "Flashcard Engine Test")
target_sources(flashcard_engine_test PRIVATE tests/engine_test.cpp)
target_compile_definitions(flashcard_engine_test PRIVATE SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample_decks")
target_compile_definitions(flashcard_engine_test PRIVATE
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(flashcard_engine_test PRIVATE
    flashcard_engine juce::juce_recommended_config_flags)

# GUI app.
# Window icon is embedded as binary data. ICON_BIG/SMALL become the exe/app icon.
juce_add_binary_data(flashcard_assets SOURCES assets/icon_256.png)
juce_add_gui_app(flashcard_quiz_gui
    PRODUCT_NAME "Flashcard Quiz"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_256.png")
target_sources(flashcard_quiz_gui PRIVATE src/gui/GuiMain.cpp)
target_compile_definitions(flashcard_quiz_gui PRIVATE
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(flashcard_quiz_gui PRIVATE
    flashcard_engine
    flashcard_assets
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
