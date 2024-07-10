cmake_minimum_required(VERSION 3.10)
project(FlaskCPP)

# Set the C standard and enable languages
enable_language(C CXX)
set(CMAKE_C_STANDARD 20)
set(CMAKE_CXX_STANDARD 20)

# Find clang
find_program(CLANG clang)
if(NOT CLANG)
  message(FATAL_ERROR "clang not found!")
endif()

option(ENABLE_BENCH "Enable benchmarking code" OFF)
if(ENABLE_BENCH)
    set(BENCH_FLAGS "-DDO_BENCH")
    add_definitions(-DDO_BENCH)
endif()

# source files
set(SOURCE_FILES flask.cpp utils/extract_cgid.cpp utils/monitor_ra.cpp utils/monitor_proc.cpp)

# include files
include_directories(includes)

# compile ebpf programs
set(EBPF_SOURCE_DIR ${CMAKE_SOURCE_DIR}/monitors)
set(EBPF_OUTPUT_DIR ${EBPF_SOURCE_DIR})

# monitor_proc.ebpf.o
add_custom_command(
    OUTPUT ${EBPF_OUTPUT_DIR}/monitor_proc.ebpf.o
    COMMAND ${CLANG} -g -O2 ${BENCH_FLAGS} -target bpf -c ${EBPF_SOURCE_DIR}/monitor_proc.ebpf.c -o ${EBPF_OUTPUT_DIR}/monitor_proc.ebpf.o
    DEPENDS ${EBPF_SOURCE_DIR}/monitor_proc.ebpf.c
)

# monitor_ra.ebpf.o
add_custom_command(
    OUTPUT ${EBPF_OUTPUT_DIR}/monitor_ra.ebpf.o
    COMMAND ${CLANG} -g -O2 ${BENCH_FLAGS} -target bpf -c ${EBPF_SOURCE_DIR}/monitor_ra.ebpf.c -o ${EBPF_OUTPUT_DIR}/monitor_ra.ebpf.o
    DEPENDS ${EBPF_SOURCE_DIR}/monitor_ra.ebpf.c
)


add_custom_target(ebpf_programs ALL
    DEPENDS ${EBPF_OUTPUT_DIR}/monitor_proc.ebpf.o ${EBPF_OUTPUT_DIR}/monitor_ra.ebpf.o
)

# executable for project
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
add_dependencies(${PROJECT_NAME} ebpf_programs)

# Link libbpf
target_link_libraries(${PROJECT_NAME} bpf)