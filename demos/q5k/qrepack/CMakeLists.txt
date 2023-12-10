CMAKE_MINIMUM_REQUIRED(VERSION 2.9)
PROJECT(qrepack)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/LibSL-small/src/
  ${CMAKE_CURRENT_SOURCE_DIR}/LibSL-small/src/LibSL/
)

link_directories(
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

else()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --static")
endif()

add_definitions(-DSRC_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

SET(SOURCES
  qrepack.cc
	tga.cc
	tga.h
  LibSL-small/src/LibSL/Math/Math.cpp
  LibSL-small/src/LibSL/Math/Vertex.cpp
  LibSL-small/src/LibSL/CppHelpers/CppHelpers.cpp
  LibSL-small/src/LibSL/System/System.cpp
)

ADD_EXECUTABLE(qrepack ${SOURCES})

IF(WIN32)
TARGET_LINK_LIBRARIES(qrepack shlwapi)
ENDIF()
