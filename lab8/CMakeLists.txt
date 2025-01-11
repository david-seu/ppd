cmake_minimum_required(VERSION 3.10)
project(DSM_Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find MPI
find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

# Add executable
add_executable(dsm_executable
    main.cpp
    Variable.cpp
    DistributedSharedMemory.cpp
    mpi_comms.cpp
)

# Link MPI libraries
target_link_libraries(dsm_executable PRIVATE ${MPI_LIBRARIES})

# Enable MPI threading support if necessary
set_target_properties(dsm_executable PROPERTIES
    CXX_EXTENSIONS OFF
)
