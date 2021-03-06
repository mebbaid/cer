# Copyright (C) 2007-2015 Istituto Italiano di Tecnologia ADVR & iCub Facility & RBCS Department
# Authors: Alberto Cardellino
# CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

option(GAZEBO_TRIPOD_PLUGIN "Compile Gazebo Yarp Plugin to control the tripod device" OFF)

if(GAZEBO_TRIPOD_PLUGIN)
  find_package(Gazebo)
  find_package(GazeboYarpPlugins)

  if(NOT Gazebo_FOUND)
    message(SEND_ERROR "Gazebo was not found therefore Gazebo plugins cannot be compiled.")
    return()
  endif()

  if(NOT GazeboYarpPlugins_FOUND)
    message(SEND_ERROR "Gazebo YARP Plugins was not found therefore Gazebo plugins cannot be compiled.")
    return()
  endif()

  # Add Gazebo CXX flags, to support Gazebo 6 reckless dependency on C++11
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GAZEBO_CXX_FLAGS}")


  set(gazeboTripod_source     GazeboYarpTripod.cpp
                              GazeboTripodMotionControl.cpp
                              GazeboTripod_Implementation.cpp)

  set(gazeboTripod_headers    GazeboYarpTripod.h
                              GazeboTripodMotionControl.h )

  get_property(cer_kinematics_INCLUDE_DIRS
               TARGET cer_kinematics
               PROPERTY BUILD_INTERFACE_INCLUDE_DIRECTORIES)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                      ${YARP_INCLUDE_DIRS}
                      ${Boost_INCLUDE_DIRS}
                      ${GAZEBO_INCLUDE_DIRS}
                      ${GazeboYarpPlugins_INCLUDE_DIRS}
                      ${cer_kinematics_INCLUDE_DIRS}
                      ${CMAKE_CURRENT_SOURCE_DIR}/../cermod/tripod)

  link_directories(${GAZEBO_LIBRARY_DIRS})
  add_library(gazeboTripod ${gazeboTripod_source} ${gazeboTripod_headers})

  target_link_libraries(gazeboTripod GazeboYarpPlugins::singleton
                                     cer_kinematics
                                     ${YARP_LIBRARIES}
                                     ${GAZEBO_LIBRARIES}
                                     ${Boost_LIBRARIES})

  yarp_install(TARGETS gazeboTripod
               COMPONENT Runtime
               LIBRARY DESTINATION  ${CMAKE_INSTALL_LIBDIR}
               ARCHIVE DESTINATION  ${CMAKE_INSTALL_LIBDIR})
endif()
