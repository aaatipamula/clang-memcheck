cmake_minimum_required(VERSION 3.13.4)
project(MemCheck)

# Find LLVM and Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Found Clang ${CLANG_PACKAGE_VERSION}")

# Include directories
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

# Add definitions
add_definitions(${LLVM_DEFINITIONS})

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS specific: Disable RTTI to match LLVM
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Tool executable
add_executable(memory-analyzer
    src/MemCheck.cpp
)

# Link against the unified Clang library
target_link_libraries(memory-analyzer
    clang-cpp
    LLVM
)

# macOS specific: May need to link against system libraries
if (APPLE)
    target_link_libraries(memory-analyzer
        "-lz"
        "-lcurses"
    )
endif()
