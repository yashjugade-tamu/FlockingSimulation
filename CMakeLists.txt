cmake_minimum_required(VERSION 3.15)

project(StreetlightSwarm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAYLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(RAYLIB_LIBRARY "${CMAKE_SOURCE_DIR}/lib/libraylib.a")

add_executable(StreetlightSwarm src/main.cpp)
target_include_directories(StreetlightSwarm PRIVATE "${RAYLIB_INCLUDE_DIR}")
target_link_libraries(StreetlightSwarm PRIVATE
    "${RAYLIB_LIBRARY}"
    opengl32 gdi32 winmm user32 shell32
)

if(EXISTS "${CMAKE_SOURCE_DIR}/lib/raylib.dll")
    add_custom_command(TARGET StreetlightSwarm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/raylib.dll"
        "$<TARGET_FILE_DIR:StreetlightSwarm>/raylib.dll")
endif()
