project( Masterserver )
cmake_minimum_required( VERSION 2.4 )

include( CheckFunctionExists )
include( CheckCXXCompilerFlag )

# Use the highest C++ standard available since VS2015 compiles with C++14
# but we only require C++11.  The recommended way to do this in CMake is to
# probably to use target_compile_features, but I don't feel like maintaining
# a list of features we use.
CHECK_CXX_COMPILER_FLAG( "-std=c++14" CAN_DO_CPP14 )
if ( CAN_DO_CPP14 )
	set ( CMAKE_CXX_FLAGS "-std=c++14 ${CMAKE_CXX_FLAGS}" )
else ()
	CHECK_CXX_COMPILER_FLAG( "-std=c++1y" CAN_DO_CPP1Y )
	if ( CAN_DO_CPP1Y )
		set ( CMAKE_CXX_FLAGS "-std=c++1y ${CMAKE_CXX_FLAGS}" )
	else ()
		CHECK_CXX_COMPILER_FLAG( "-std=c++11" CAN_DO_CPP11 )
		if ( CAN_DO_CPP11 )
			set ( CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}" )
		else ()
			CHECK_CXX_COMPILER_FLAG( "-std=c++0x" CAN_DO_CPP0X )
			if ( CAN_DO_CPP0X )
				set ( CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}" )
			endif ()
		endif ()
	endif ()
endif ()

set( ZAN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src )
include_directories( ${ZAN_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if( NOT STRNICMP_EXISTS )
   add_definitions( -Dstrnicmp=strncasecmp )
endif( NOT STRNICMP_EXISTS )

add_executable( master-97
	main.cpp
	network.cpp
	${ZAN_DIR}/gitinfo.cpp
	${ZAN_DIR}/networkshared.cpp
	${ZAN_DIR}/platform.cpp
	${ZAN_DIR}/huffman/bitreader.cpp 
	${ZAN_DIR}/huffman/bitwriter.cpp 
	${ZAN_DIR}/huffman/huffcodec.cpp 
	${ZAN_DIR}/huffman/huffman.cpp
)

add_dependencies( master-97 revision_check )

if( WIN32 )
	target_link_libraries( master-97 ws2_32 winmm )
endif( WIN32 )
