cmake_minimum_required( VERSION 2.4 )

get_target_property(ZIPDIR_EXE zipdir LOCATION)

add_pk3(skulltag_actors.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static)
