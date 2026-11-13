cmake_minimum_required(VERSION 3.21)

project(ZXFont VERSION 0.0.4 LANGUAGES CXX)

add_library(zxfont STATIC src/zxfont.cpp src/font.cpp)
add_library(ZXFont::zxfont ALIAS zxfont)

target_compile_features(zxfont PUBLIC cxx_std_20)
target_include_directories(zxfont PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(ZXFONT_BUILD_EXAMPLES "Build ZXFont examples" ${PROJECT_IS_TOP_LEVEL})
option(ZXFONT_BUILD_TESTS "Build ZXFont tests" ${PROJECT_IS_TOP_LEVEL})

if(ZXFONT_BUILD_EXAMPLES)
    add_executable(zxfont_basic examples/basic/main.cpp)
    target_link_libraries(zxfont_basic PRIVATE ZXFont::zxfont)
endif()

if(ZXFONT_BUILD_TESTS)
    include(CTest)
    add_executable(zxfont_tests tests/test_zxfont.cpp)
    target_link_libraries(zxfont_tests PRIVATE ZXFont::zxfont)
    add_test(NAME zxfont_core COMMAND zxfont_tests)
endif()
