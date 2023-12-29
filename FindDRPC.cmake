# - Find Discord Rich Presence
# Find the native DRPC includes and library
#
#  DRPC_INCLUDE_DIR - where to find discord.h
#  DRPC_LIBRARIES   - List of libraries when using DRPC.
#  DRPC_FOUND       - True if DRPC found.

IF(DRPC_INCLUDE_DIR AND DRPC_LIBRARIES)
  # Already in cache, be silent
  SET(DRPC_FIND_QUIETLY TRUE)
ENDIF(DRPC_INCLUDE_DIR AND DRPC_LIBRARIES)

FIND_PATH(DRPC_INCLUDE_DIR discord.h
          PATHS "${DRPC_DIR}" ENV DRPC_DIR
          )

FIND_LIBRARY(DRPC_LIBRARIES NAMES drpc discord_game_sdk.dll.lib
             PATHS "${DRPC_DIR}" ENV DRPC_DIR
             )

# MARK_AS_ADVANCED(DRPC_LIBRARIES DRPC_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set DRPC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DRPC DEFAULT_MSG DRPC_LIBRARIES DRPC_INCLUDE_DIR)
