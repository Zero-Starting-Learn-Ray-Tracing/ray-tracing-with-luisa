cmake_minimum_required(VERSION 3.26...3.29)


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if (PROJECT_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
    message(
        FATAL_ERROR
        "The binary directory of CMake cannot be the same as source directory!"
        "use 'cmake -B build' to specify a different binary directory."
    )
endif()


project(
    RayTracing
    LANGUAGES C CXX
    DESCRIPTION "ray tracing with luisa compute"
    HOMEPAGE_URL https://github.com/Zero-Starting-Learn-Ray-Tracing/zero
    VERSION 0.0.1
)

# you may set the LUISA_COMPUTE_ENABLE_XXX options before including LuisaCompute
add_subdirectory(thirdparty/LuisaCompute)

# setup the output directories to make the executables under the same binary directory as LuisaCompute
include(thirdparty/LuisaCompute/scripts/setup_output_dirs.cmake)


# add your executables
# add_executable(test src/test.cpp)
# target_include_directories(test PUBLIC thirdparty/LuisaCompute/include)
# target_link_libraries(test PRIVATE luisa::compute)

# add_executable(test2 src/test2.cpp)
# target_include_directories(test2 PUBLIC thirdparty/LuisaCompute/include)
# target_link_libraries(test2 PRIVATE luisa::compute)

add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    thirdparty/LuisaCompute/include
    thirdparty/cxxopts/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE luisa::compute)
