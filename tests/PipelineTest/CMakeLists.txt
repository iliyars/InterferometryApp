cmake_minimum_required(VERSION 3.20)
project(PipelineTest LANGUAGES CXX)

add_executable(PipelineTest
    PipelineTest.cpp
)

target_link_libraries(PipelineTest PRIVATE InterferometryCore)

if(WIN32)
    add_custom_command(TARGET PipelineTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${OpenCV_DLL_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${OpenCV_DLL_RELEASE}>"
            $<TARGET_FILE_DIR:PipelineTest>
        COMMENT "Copying OpenCV DLL..."
    )
endif()
