# tests/PipelineTest/CMakeLists.txt
# Консольный тест pipeline: загрузка → границы → трассировка → аппроксимация → CSV

cmake_minimum_required(VERSION 3.20)
project(PipelineTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Корень проекта (два уровня вверх от tests/PipelineTest) ---
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# --- OpenCV (локальная копия в external/) ---
set(OPENCV_ROOT "${PROJECT_ROOT}/external/opencv")
set(OpenCV_INCLUDE_DIRS "${OPENCV_ROOT}")
set(OpenCV_LIBS "${OPENCV_ROOT}/lib/opencv_world4120d.lib")
set(OpenCV_DLL "${PROJECT_ROOT}/build/x64/Debug/opencv_world4120d.dll")

# --- Исходники Core (из основного проекта) ---
set(CORE_INCLUDE "${PROJECT_ROOT}/include/Core")
set(COMMON_INCLUDE "${PROJECT_ROOT}/include/Common")
set(CORE_SRC "${PROJECT_ROOT}/src/Core")

set(CORE_SOURCES
    ${CORE_SRC}/Tracing/EllipseBoundary.cpp
    ${CORE_SRC}/Tracing/FringeTracer.cpp
    ${CORE_SRC}/Tracing/ImageLoader.cpp
    ${CORE_SRC}/Tracing/PolynomialApproximator.cpp
)

set(CORE_HEADERS
    ${CORE_INCLUDE}/Tracing/EllipseBoundary.h
    ${CORE_INCLUDE}/Tracing/FringeTracer.h
    ${CORE_INCLUDE}/Tracing/ImageLoader.h
    ${CORE_INCLUDE}/Tracing/PolynomialApproximator.h
    ${CORE_INCLUDE}/Common/TracingTypes.h
    ${COMMON_INCLUDE}/Types.h
)

# --- Заглушка pch.h для консольной сборки ---
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/pch.h"
     "// Заглушка pch.h для консольной сборки (без MFC)\n#pragma once\n")

# --- Исполняемый файл ---
add_executable(PipelineTest
    PipelineTest.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(PipelineTest PRIVATE
    ${CORE_INCLUDE}
    ${CORE_INCLUDE}/Tracing
    ${COMMON_INCLUDE}
    ${PROJECT_ROOT}/include
    ${CMAKE_CURRENT_BINARY_DIR}   # для заглушки pch.h
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(PipelineTest PRIVATE ${OpenCV_LIBS})

# --- Копировать OpenCV DLL рядом с exe ---
if(WIN32 AND EXISTS "${OpenCV_DLL}")
    add_custom_command(TARGET PipelineTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DLL}"
            $<TARGET_FILE_DIR:PipelineTest>
    )
endif()
