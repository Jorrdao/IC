cmake_minimum_required(VERSION 3.16)
project(bit_stream)

# ----------------------------
# Configurações de compilação
# ----------------------------
set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

# ----------------------------
# Diretórios e saídas
# ----------------------------
set(BASE_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/../bin)

# ----------------------------
# Biblioteca comum (bit/byte stream)
# ----------------------------
add_library(Common OBJECT)
target_sources(Common PRIVATE bit_stream.cpp byte_stream.cpp)
target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------
# Executáveis principais
# ----------------------------
add_executable(text2bin text2bin.cpp $<TARGET_OBJECTS:Common>)
add_executable(bin2text bin2text.cpp $<TARGET_OBJECTS:Common>)

# ----------------------------
# Executáveis do projeto (Parte II)
# ----------------------------
add_executable(wav_quant_enc wav_quant_enc.cpp $<TARGET_OBJECTS:Common>)
add_executable(wav_quant_dec wav_quant_dec.cpp $<TARGET_OBJECTS:Common>)
