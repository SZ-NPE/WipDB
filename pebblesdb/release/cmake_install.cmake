# Install script for directory: /mnt/shunzi/lsm/WipDB/pebblesdb

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/mnt/shunzi/lsm/WipDB/pebblesdb/release/libpebblesdb.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/pebblesdb" TYPE FILE FILES
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/c.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/cache.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/comparator.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/db.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/env.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/filter_policy.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/iterator.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/options.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/slice.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/replay_iterator.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/status.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/table_builder.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/table.h"
    "/mnt/shunzi/lsm/WipDB/pebblesdb/src/include/pebblesdb/write_batch.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/mnt/shunzi/lsm/WipDB/pebblesdb/release/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
