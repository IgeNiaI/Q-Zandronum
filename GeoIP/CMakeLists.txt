cmake_minimum_required( VERSION 2.4 )

add_library( GeoIP GeoIP.c )

if( WIN32 )
  target_link_libraries( GeoIP ws2_32 ) 
endif ( )