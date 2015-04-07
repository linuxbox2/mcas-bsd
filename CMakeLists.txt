cmake_minimum_required(VERSION 2.8.11)

project(Mcas)
set(VERSION "0.00")

include_directories(
#  ${PROJECT_BINARY_DIR}/src/include
#  ${OFED_PREFIX}/include
#  ${LEVELDB_PREFIX}/include
)

link_directories(
#  ${OFED_PREFIX}/lib
#  ${LEVELDB_PREFIX}/lib
)

#Check Includes
# include(CheckIncludeFiles)
# include(CheckIncludeFileCXX)
# include(CheckFunctionExists)


# include(CheckSymbolExists)

# Now create a useable config.h
# configure_file(
#   ${PROJECT_SOURCE_DIR}/src/include/config-h.in.cmake
#   ${PROJECT_BINARY_DIR}/include/acconfig.h
# )
# include_directories(${PROJECT_BINARY_DIR}/include)

find_package(Threads REQUIRED)

set(MCAS_SHARED STATIC)


set(CLIKE_COMMON_FLAGS "-fPIC -D_POSIX_C_SOURCE=200112L -D_SVID_SOURCE=1 -Dbool_t=int -pthread")

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
   set(CMAKE_CLIKE_FLAGS "${CMAKE_CLIKE_FLAGS} -DX86_64")
endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")

# find out which platform we are building on
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(LINUX ON)
  set(UNIX ON)
  FIND_PACKAGE(Threads)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

# find out which platform we are building on
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(OperatingSystem "Mac OS X")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

set(CLIKE_COMMON_FLAGS "${CLIKE_COMMON_FLAGS} ${CMAKE_CLIKE_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CLIKE_COMMON_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLIKE_COMMON_FLAGS} -std=c++14")

list(APPEND libmcas_files
	ptst.c gc.c
	osi_mcas_obj_cache.c
	skip_cas_adt.c
# not so useful,
#	rb_stm.c mcas.c skip_mcas.c
)

add_library(libmcas_objs OBJECT ${libmcas_files})
add_library(mcas ${MCAS_SHARED} $<TARGET_OBJECTS:libmcas_objs>)

set(skip_adt_test_srcs
	skip_adt_test.c
)
add_executable(skip_adt_test ${skip_adt_test_srcs})
target_link_libraries(skip_adt_test mcas)
#
# not useful: Matt says this rb has bugs, better implementations elsewhere
#set(rb_stm_lock_srcs
#	stm_lock.c
#	set_harness.c
#)
#add_executable(rb_stm_lock ${rb_stm_lock_srcs})
#target_link_libraries(rb_stm_lock mcas)
