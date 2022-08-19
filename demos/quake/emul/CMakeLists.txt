CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(quake_emul)

include_directories(
  ${CMAKE_SOURCE_DIR}/../../software/api/
)

link_directories(
)

add_definitions(-DSRC_PATH=\"${CMAKE_SOURCE_DIR}/\")

SET(SOURCES
  emul_quake.cpp
  tga.cpp
  tga.h
)

ADD_DEFINITIONS(-DEMUL=1)
ADD_EXECUTABLE(quake_emul ${SOURCES})
