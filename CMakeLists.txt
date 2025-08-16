# cmake_minimum_required(VERSION 2.8.5 FATAL_ERROR)
# project(ccDSM)

# set (CMAKE_C_COMPILER "/usr/local/bin/mpicxx")
# set (CMAKE_CXX_COMPILER ${CMAKE_C_COMPILER})

cmake_minimum_required(VERSION 3.10)
project(ccDSM)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#Compiler options
# set(CMAKE_C_FLAGS "-Wall -Wno-deprecated-declarations -Wsign-compare -DNDEBUG -g")
set(CMAKE_C_FLAGS "-Wall -Wno-deprecated-declarations -Wsign-compare -O2 -g -fno-omit-frame-pointer -fopenmp -DNDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 ")

# Link Options
set(LINKS_FLAGS "-lpthread -libverbs -lmemcached -lthrift -lboost_filesystem -lboost_system -lboost_date_time -lboost_thread -lmpi")
set(LIBRARIES ${LIBRARIES} hdr_histogram_static numa)

option (SANITIZE "Turn on sanitization" OFF)
if (SANITIZE)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

find_package(Boost REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

set(THRIFT_SRC ${PROJECT_SOURCE_DIR}/p4src/build/gen-cpp)
set(INCLUDE_BASE ${PROJECT_SOURCE_DIR}/include)

SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/p4src/build/gen-cpp/ccDSM.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking)
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/p4src/build/gen-cpp/p4_pd_rpc_types.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking)
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/p4src/build/gen-cpp/p4_pd_rpc_types.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking)

include_directories(${INCLUDE_BASE})
include_directories(${THRIFT_SRC})

# TPCC include
set(TPCC_DIR ${PROJECT_SOURCE_DIR}/database)
include_directories(${TPCC_DIR})
include_directories(${TPCC_DIR}/bench)
include_directories(${TPCC_DIR}/storage)
include_directories(${TPCC_DIR}/tpcc)
include_directories(${TPCC_DIR}/txn)
include_directories(${TPCC_DIR}/utils)

# include custom headers
include_directories(${CMAKE_SOURCE_DIR}/include/custom_include)
# include hdr_histogram headers
include_directories(${CMAKE_SOURCE_DIR}/lib/HdrHistogram/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/HdrHistogram)
# include atomic_queue headers
include_directories(${CMAKE_SOURCE_DIR}/lib/atomic_queue/include)

#Source file define
set(COMMON_SRC ${PROJECT_SOURCE_DIR}/src)

#Used by both server and clients
file(GLOB_RECURSE COMMON_FILE ${COMMON_SRC}/*.cpp)
add_library(ccDSM STATIC ${COMMON_FILE} 
${TPCC_DIR}/tpcc/TpccRandomGenerator.cpp  
${TPCC_DIR}/txn/TransactionManager2PL.cpp
${TPCC_DIR}/utils/Profiler.cpp
${TPCC_DIR}/Meta.cpp
${TPCC_DIR}/log.cpp
${TPCC_DIR}/gallocator.cpp
)
target_compile_definitions(ccDSM PRIVATE
    LOCK
    BACKOFF
    # PROFILE
)
link_libraries(ccDSM)

file(GLOB_RECURSE THRIFT_RPC_FILE ${THRIFT_SRC}/*.cpp)
add_library(thrift_cc SHARED ${THRIFT_RPC_FILE})
link_libraries(thrift_cc)

# bench
# file(GLOB BENCHMARK_SRC ${PROJECT_SOURCE_DIR}/bench/*.cpp)
# foreach (BENCHMARK ${BENCHMARK_SRC})
    # get_filename_component(BENCHMARK_NAME ${BENCHMARK} NAME_WE)
    # add_executable(${BENCHMARK_NAME} ${BENCHMARK})
    # target_link_libraries(${BENCHMARK_NAME} ${LINKS_FLAGS})
# endforeach()

# test

file(GLOB TEST_SRC ${PROJECT_SOURCE_DIR}/test/*.cpp)
foreach (TEST ${TEST_SRC})
    get_filename_component(TEST_NAME ${TEST} NAME_WE)
    add_executable(${TEST_NAME} ${TEST})
    target_link_libraries(${TEST_NAME} ${LINKS_FLAGS} ${LIBRARIES} ${Boost_LIBRARIES})
endforeach()

add_executable(tpcc ${TPCC_DIR}/tpcc/TpccBenchmarkMain.cpp)
target_compile_definitions(tpcc PRIVATE
    LOCK
    BACKOFF
    # PROFILE
)
target_link_libraries(tpcc ${LINKS_FLAGS} ${LIBRARIES})



