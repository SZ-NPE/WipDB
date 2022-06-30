# Install script for directory: /mnt/shunzi/lsm/WipDB/kv/src

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/db/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/table/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/util/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/port/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/env/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/monitoring/cmake_install.cmake")
  include("/mnt/shunzi/lsm/WipDB/kv/release/src/memtable/cmake_install.cmake")

endif()

