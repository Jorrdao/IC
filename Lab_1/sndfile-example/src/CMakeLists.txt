cmake_minimum_required(VERSION 3.10)
project(AudioEffects)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(FFTW3 REQUIRED fftw3)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

# Add all source files
file(GLOB SOURCES "*.cpp")

# Create executable for each source file
foreach(SOURCE ${SOURCES})
    get_filename_component(EXECUTABLE_NAME ${SOURCE} NAME_WE)
    add_executable(${EXECUTABLE_NAME} ${SOURCE})
    target_link_libraries(${EXECUTABLE_NAME} ${SNDFILE_LIBRARIES})
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${SNDFILE_INCLUDE_DIRS})
    
    # Add FFTW linking for wav_dct specifically
    if(${EXECUTABLE_NAME} STREQUAL "wav_dct")
        target_link_libraries(${EXECUTABLE_NAME} ${FFTW3_LIBRARIES})
        target_include_directories(${EXECUTABLE_NAME} PRIVATE ${FFTW3_INCLUDE_DIRS})
    endif()
endforeach()