CMAKE_MINIMUM_REQUIRED ( VERSION 2.4 )

INCLUDE ( CheckFunctionExists )

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
IF( NOT STRNICMP_EXISTS )
	ADD_DEFINITIONS ( -Dstrnicmp=strncasecmp )
ENDIF( NOT STRNICMP_EXISTS )

INCLUDE_DIRECTORIES ( ${CMAKE_CURRENT_SOURCE_DIR} )

FILE ( GLOB HDRS *.h )
FILE ( GLOB SRCS *.cpp )
SET ( SRCS ${SRCS} ../src/huffman/bitwriter.cpp ../src/huffman/huffcodec.cpp ../src/huffman/huffman.cpp ../src/networkshared.cpp ../src/platform.cpp )

ADD_DEFINITIONS ( -DNO_GUI )

ADD_EXECUTABLE ( statsmaker ${HDRS} ${SRCS} )
IF ( WIN32 )
  TARGET_LINK_LIBRARIES( statsmaker wsock32 )
ENDIF ( WIN32 )
