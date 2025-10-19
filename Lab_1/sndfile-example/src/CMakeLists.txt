cmake_minimum_required(VERSION 3.16)
project(sndfile-example)

SET (CMAKE_BUILD_TYPE "Release")
#SET (CMAKE_BUILD_TYPE "Debug")

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++20")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3")
SET (CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

SET (BASE_DIR ${CMAKE_SOURCE_DIR} )
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/../bin)

add_library(Common OBJECT)

target_sources(Common PRIVATE bit_stream.cpp byte_stream.cpp)

add_executable (wav_cp wav_cp.cpp)
target_link_libraries (wav_cp sndfile)

add_executable (wav_hist wav_hist.cpp)
target_link_libraries (wav_hist sndfile)

add_executable (wav_dct wav_dct.cpp)
target_link_libraries (wav_dct sndfile fftw3)

add_executable (wav_quant wav_quant.cpp)
target_link_libraries (wav_quant sndfile)

add_executable (wav_effects wav_effects.cpp)
target_link_libraries (wav_effects sndfile)

add_executable (wav_cmp wav_cmp.cpp)
target_link_libraries (wav_cmp sndfile)

add_executable (wav_mono wav_mono.cpp)
target_link_libraries (wav_mono sndfile)

add_executable (wav_dct_enc wav_dct_enc.cpp)
target_link_libraries (wav_dct_enc sndfile fftw3 Common)

add_executable (wav_dct_dec wav_dct_dec.cpp)
target_link_libraries (wav_dct_dec sndfile fftw3 Common)

add_executable(collect_data collect_data.cpp)