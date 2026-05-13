cmake_minimum_required(VERSION 3.20)
project(InterferometryApp LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source build not allowed.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- OpenCV ---
set(OPENCV_ROOT "${CMAKE_SOURCE_DIR}/external/opencv")

set(OpenCV_LIBS
    "$<$<CONFIG:Debug>:${OPENCV_ROOT}/lib/opencv_world4120d.lib>"
    "$<$<NOT:$<CONFIG:Debug>>:${OPENCV_ROOT}/lib/opencv_world4120.lib>"
)
set(OpenCV_DLL_DEBUG   "${OPENCV_ROOT}/bin/opencv_world4120d.dll")
set(OpenCV_DLL_RELEASE "${OPENCV_ROOT}/bin/opencv_world4120.dll")

# pch-заглушка для сборки без MFC
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/pch.h"
    "// Stub pch.h for non-MFC build\n#pragma once\n")

# --- Библиотека ядра ---
add_library(InterferometryCore STATIC
    src/Core/Tracing/EllipseBoundary.cpp
    src/Core/Tracing/FringeTracer.cpp
    src/Core/Tracing/ImageLoader.cpp
    src/Core/Tracing/PolynomialApproximator.cpp
    src/Core/Tracing/FringeSkeletonizer.cpp
)

target_include_directories(InterferometryCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/Core
    ${CMAKE_SOURCE_DIR}/include/Core/Tracing
    ${CMAKE_SOURCE_DIR}/include/Common
    ${OPENCV_ROOT}
    ${CMAKE_CURRENT_BINARY_DIR}   # pch.h заглушка
)

target_link_libraries(InterferometryCore PUBLIC ${OpenCV_LIBS})

# --- Тесты ---
# OpenCV_DLL_* пробрасываем в дочерний CMakeLists через cache
set(OpenCV_DLL_DEBUG   "${OpenCV_DLL_DEBUG}"   CACHE INTERNAL "")
set(OpenCV_DLL_RELEASE "${OpenCV_DLL_RELEASE}" CACHE INTERNAL "")

add_subdirectory(tests/PipelineTest)
