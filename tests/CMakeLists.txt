# Фикстуры images/ встраиваются только в тестовые EXE, не в приложение.
file(GLOB IMAGEPUZZLE_TEST_IMAGES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/images/*")
if(NOT IMAGEPUZZLE_TEST_IMAGES)
    message(FATAL_ERROR "The images directory must contain test fixtures when BUILD_TESTING=ON.")
endif()
function(imagepuzzle_add_samples target)
    imagepuzzle_add_assets(${target})
    qt_add_resources(${target} "${target}_fixtures" PREFIX "/imagepuzzle"
        BASE "${PROJECT_SOURCE_DIR}" FILES ${IMAGEPUZZLE_TEST_IMAGES})
endfunction()

qt_add_executable(ImagePuzzleResourceTests integration/tst_sampleimages.cpp)
target_link_libraries(ImagePuzzleResourceTests PRIVATE ImagePuzzleCore Qt6::Test)
imagepuzzle_add_samples(ImagePuzzleResourceTests)
add_test(NAME integration.images COMMAND ImagePuzzleResourceTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/images.xml,junitxml" -o -,txt)
set_tests_properties(integration.images PROPERTIES LABELS "integration" TIMEOUT 120)

qt_add_executable(ImagePuzzleGameTests integration/tst_gamecontroller.cpp
    ../src/app/appcontroller.cpp ../src/app/appcontroller.h
    ../src/app/gamecontroller.cpp ../src/app/gamecontroller.h)
target_link_libraries(ImagePuzzleGameTests PRIVATE ImagePuzzleCore Qt6::Test)
add_test(NAME integration.gamecontroller COMMAND ImagePuzzleGameTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/gamecontroller.xml,junitxml" -o -,txt)
set_tests_properties(integration.gamecontroller PROPERTIES LABELS "integration" TIMEOUT 60)

qt_add_executable(ImagePuzzleCoreTests unit/tst_puzzle.cpp)
target_link_libraries(ImagePuzzleCoreTests PRIVATE ImagePuzzleCore Qt6::Test)
add_test(NAME unit.puzzle COMMAND ImagePuzzleCoreTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/puzzle.xml,junitxml" -o -,txt)
set_tests_properties(unit.puzzle PROPERTIES LABELS "unit" TIMEOUT 60)

qt_add_executable(ImagePuzzleStateTests unit/tst_appcontroller.cpp ../src/app/appcontroller.cpp ../src/app/appcontroller.h)
target_include_directories(ImagePuzzleStateTests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(ImagePuzzleStateTests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME unit.appcontroller COMMAND ImagePuzzleStateTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/appcontroller.xml,junitxml" -o -,txt)
set_tests_properties(unit.appcontroller PROPERTIES LABELS "unit" TIMEOUT 30)

qt_add_executable(ImagePuzzleWindowTests ui/tst_mainwindow.cpp)
target_link_libraries(ImagePuzzleWindowTests PRIVATE ImagePuzzleUi Qt6::Test)
imagepuzzle_add_samples(ImagePuzzleWindowTests)
add_test(NAME ui.mainwindow COMMAND ImagePuzzleWindowTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/mainwindow.xml,junitxml" -o -,txt)
set_tests_properties(ui.mainwindow PROPERTIES LABELS "ui" TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
