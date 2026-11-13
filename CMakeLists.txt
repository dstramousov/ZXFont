cmake_minimum_required(VERSION 3.21)

project(ZXFont VERSION 0.0.9 LANGUAGES CXX)

add_library(zxfont STATIC src/zxfont.cpp src/font.cpp src/palette.cpp)
add_library(ZXFont::zxfont ALIAS zxfont)

target_compile_features(zxfont PUBLIC cxx_std_20)
target_include_directories(zxfont PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(ZXFONT_BUILD_EXAMPLES "Build ZXFont headless examples" ${PROJECT_IS_TOP_LEVEL})
option(ZXFONT_BUILD_TESTS "Build ZXFont tests" ${PROJECT_IS_TOP_LEVEL})
option(ZXFONT_BUILD_DEMOS "Build the interactive SDL3 visual test applications" ${PROJECT_IS_TOP_LEVEL})
option(ZXFONT_FETCH_SDL3 "Download pinned SDL3 if no system installation is found" ${PROJECT_IS_TOP_LEVEL})

# The Unicode font is a data asset used only by demos/tests. Consumers
# linking ZXFont::zxfont are not required to copy or install it.
if(ZXFONT_BUILD_DEMOS OR ZXFONT_BUILD_TESTS)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/zx-spectrum-unicode-8x8.psf"
        "${CMAKE_CURRENT_BINARY_DIR}/fonts/zx-spectrum-unicode-8x8.psf"
        COPYONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/zx-compact-airy-6x8.psf"
        "${CMAKE_CURRENT_BINARY_DIR}/fonts/zx-compact-airy-6x8.psf"
        COPYONLY
    )
endif()

if(ZXFONT_BUILD_EXAMPLES)
    add_executable(zxfont_basic examples/basic/main.cpp)
    target_link_libraries(zxfont_basic PRIVATE ZXFont::zxfont)
endif()

if(ZXFONT_BUILD_DEMOS)
    # SDL3 is a dependency of the demos only, never of ZXFont::zxfont.
    find_package(SDL3 3.2 QUIET CONFIG)
    if(NOT TARGET SDL3::SDL3)
        if(NOT ZXFONT_FETCH_SDL3)
            message(FATAL_ERROR
                "SDL3 >= 3.2 is required for the visual demos. "
                "Install SDL3 or configure -DZXFONT_FETCH_SDL3=ON; "
                "use -DZXFONT_BUILD_DEMOS=OFF for a headless build.")
        endif()
        include(FetchContent)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3)
    endif()

    foreach(demo IN ITEMS kaya mode256)
        add_executable(zxfont_${demo}
            examples/${demo}/main.cpp
            examples/common/sdl_display.cpp
        )
        target_link_libraries(zxfont_${demo} PRIVATE ZXFont::zxfont SDL3::SDL3)
    endforeach()
endif()

if(ZXFONT_BUILD_TESTS)
    include(CTest)
    add_executable(zxfont_tests tests/test_zxfont.cpp)
    target_link_libraries(zxfont_tests PRIVATE ZXFont::zxfont)
    target_compile_definitions(zxfont_tests PRIVATE
        ZXFONT_TEST_FONT_PATH="${CMAKE_CURRENT_BINARY_DIR}/fonts/zx-spectrum-unicode-8x8.psf"
        ZXFONT_TEST_COMPACT_PATH="${CMAKE_CURRENT_BINARY_DIR}/fonts/zx-compact-airy-6x8.psf")
    add_test(NAME zxfont_core COMMAND zxfont_tests)

    add_executable(zxfont_demo_tests tests/test_demo_support.cpp)
    target_link_libraries(zxfont_demo_tests PRIVATE ZXFont::zxfont)
    add_test(NAME zxfont_demo_support COMMAND zxfont_demo_tests)
endif()
