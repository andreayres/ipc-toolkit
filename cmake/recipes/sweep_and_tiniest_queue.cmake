if(TARGET STQ::CPU)
    return()
endif()

message(STATUS "Third-party: creating target 'STQ::CPU'")

option(STQ_WITH_CPU  "Enable CPU Implementation"   ON)
option(STQ_WITH_CUDA "Enable CUDA Implementation" OFF) # get this through GPU_CCD

if(EXISTS "${IPC_TOOLKIT_STQ_PATH}")
    message(STATUS "Using STQ found at: ${IPC_TOOLKIT_STQ_PATH}")
    add_subdirectory("${IPC_TOOLKIT_STQ_PATH}" "${PROJECT_BINARY_DIR}/sweep_and_tiniest_queue")
else()
    include(FetchContent)
    FetchContent_Declare(
        sweep_and_tiniest_queue
        GIT_REPOSITORY https://github.com/dbelgrod/broadphase-gpu.git
        GIT_TAG 6b8edb7a85fc8aec5d4a7846632eb40dad7e4e74
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(sweep_and_tiniest_queue)
endif()