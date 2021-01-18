cmake_minimum_required(VERSION 3.4.3)

project("ABSTRACT-RESOURCE")


message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/gatlin/include ${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${LLVM_LIBRARY_DIRS})


list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fPIC -Wall -std=c++14")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fPIC -Wall -std=c++11 -fsanitize=address")

add_llvm_library( libresource_collect MODULE
    ResourceCollectPass.cpp
    PexCallGraph.cpp
    Utils.cpp
    gatlin/gatlin.cpp
    LockPair.cpp
    FindLockAndAtomic.cpp
    LockAtomicStructure.cpp
    CtlTableAnalysis.cpp
    
    DEPENDS
    PLUGIN_TOOL
    opt
    )

target_compile_features(libresource_collect PRIVATE cxx_range_for cxx_auto_type)

if ( CMAKE_SYSTEM_NAME MATCHES "Darwin")
    llvm_map_components_to_libnames(llvm_libs bitwriter
        core ipo irreader instcombine instrumentation target linker analysis
        scalaropts support )
    target_link_libraries( libresource_collect ${llvm_libs})
else()
    target_link_libraries( libresource_collect )
endif()