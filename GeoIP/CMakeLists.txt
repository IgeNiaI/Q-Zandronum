cmake_minimum_required( VERSION 2.4 )
include( CheckFunctionExists )

if( NOT WIN32 )
  CHECK_FUNCTION_EXISTS( gettimeofday GETTIMEOFDAY_EXISTS )
  if( GETTIMEOFDAY_EXISTS )
    add_definitions( -DHAVE_GETTIMEOFDAY=1 )
  endif( GETTIMEOFDAY_EXISTS )
endif( NOT WIN32 )

add_library( GeoIP GeoIP.c )

if( WIN32 )
  target_link_libraries( GeoIP ws2_32 ) 
endif ( )