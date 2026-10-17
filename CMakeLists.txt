cmake_minimum_required(VERSION 3.16)
project(ImagePuzzle VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_library(ImagePuzzleCore STATIC
    src/core/puzzlegenerator.h src/core/puzzlegenerator.cpp
    src/core/puzzlemodel.h src/core/puzzlemodel.cpp
    src/io/imageloader.h src/io/imageloader.cpp
    src/io/sampleimages.h src/io/sampleimages.cpp
)
target_include_directories(ImagePuzzleCore PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ImagePuzzleCore PUBLIC Qt6::Gui)

qt_add_library(ImagePuzzleUi STATIC
    src/ui/expeditiontheme.h src/ui/expeditiontheme.cpp
    src/app/appcontroller.h src/app/appcontroller.cpp
    src/app/gamecontroller.h src/app/gamecontroller.cpp
    src/ui/mainwindow.h src/ui/mainwindow.cpp src/ui/mainwindow.ui
    src/ui/loadpage.h src/ui/loadpage.cpp
    src/ui/gamepage.h src/ui/gamepage.cpp
    src/ui/imagepreview.h src/ui/imagepreview.cpp
    src/ui/puzzlescene.h src/ui/puzzlescene.cpp
    src/ui/puzzleview.h src/ui/puzzleview.cpp
)
target_include_directories(ImagePuzzleUi PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ImagePuzzleUi PUBLIC ImagePuzzleCore Qt6::Widgets PRIVATE Qt6::Concurrent)

qt_add_executable(ImagePuzzle src/app/main.cpp src/app/selfcheck.h src/app/selfcheck.cpp)
if(WIN32)
    enable_language(RC)
    configure_file(assets/icons/imagepuzzle.rc.in "${CMAKE_CURRENT_BINARY_DIR}/imagepuzzle.rc" @ONLY)
    target_sources(ImagePuzzle PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/imagepuzzle.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/imagepuzzle.rc" APPEND PROPERTY
        OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/assets/icons/imagepuzzle.ico")
endif()
target_link_libraries(ImagePuzzle PRIVATE ImagePuzzleUi)
target_compile_definitions(ImagePuzzle PRIVATE IMAGEPUZZLE_VERSION="${PROJECT_VERSION}")
set_target_properties(ImagePuzzle PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

function(imagepuzzle_add_assets target)
    qt_add_resources(${target} "${target}_assets" PREFIX "/imagepuzzle"
        BASE "${PROJECT_SOURCE_DIR}" FILES
        "${PROJECT_SOURCE_DIR}/assets/museum/kunitsina-estate.jpg"
        "${PROJECT_SOURCE_DIR}/assets/museum/korely-summer.jpg"
        "${PROJECT_SOURCE_DIR}/assets/museum/korely-winter.jpg"
        "${PROJECT_SOURCE_DIR}/assets/fonts/Geologica.ttf"
        "${PROJECT_SOURCE_DIR}/assets/icons/imagepuzzle.ico")
endfunction()
imagepuzzle_add_assets(ImagePuzzle)
install(TARGETS ImagePuzzle BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_app_script(TARGET ImagePuzzle OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
install(FILES assets/fonts/OFL.txt DESTINATION licenses RENAME Geologica-OFL.txt)
install(FILES assets/icons/Fluent-Emoji-MIT.txt DESTINATION licenses)
install(FILES assets/SOURCES.md DESTINATION .)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
