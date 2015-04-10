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

set(MCAS_SHARED STATIC)


set(CLIKE_COMMON_FLAGS "-fPIC -D_POSIX_C_SOURCE=200112L -D_SVID_SOURCE=1 -Dbool_t=int -pthread")

# need logic to set Mcas_architecture to one of
#	SPARC	sparc_defns
#	SOLARIS_X86_686	solaris_x86_defns
#	SOLARIS_X86_AMD64	solaris_amd64_defns
#	INTEL	intel_defns
#	X86_64	amd64_defns
#	PPC	ppc_defns
#	IA64	ia64_defns
#	MIPS	mips_defns
#	ALPHA	alpha_defns
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
   set(Mcas_architecture X86_64)
   set(Mcas_arch_header amd64_defns)
endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
set(CMAKE_CLIKE_FLAGS "${CMAKE_CLIKE_FLAGS} -D${Mcas_architecture}")

find_package(Threads REQUIRED)

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

configure_file(
	"${PROJECT_SOURCE_DIR}/mcas.h.in"
	"${PROJECT_BINARY_DIR}/mcas.h"
)

add_library(libmcas_objs OBJECT ${libmcas_files})
add_library(mcas ${MCAS_SHARED} $<TARGET_OBJECTS:libmcas_objs>)
install(TARGETS mcas DESTINATION lib)
install(FILES
osi_mcas_obj_cache.h
portable_defns.h
set_queue_adt.h
gc.h
"${PROJECT_BINARY_DIR}/mcas.h"
${Mcas_arch_header}.h
DESTINATION include/mcas)

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
