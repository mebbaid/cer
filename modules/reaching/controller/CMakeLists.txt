# Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
# Author: Ugo Pattacini
# email:  ugo.pattacini@iit.it

project(cer_reaching-controller)

get_property(cer_kinematics_INCLUDE_DIRS
             TARGET cer_kinematics
             PROPERTY BUILD_INTERFACE_INCLUDE_DIRECTORIES)

include_directories(${YARP_INCLUDE_DIRS} ${ICUB_INCLUDE_DIRS} ${cer_kinematics_INCLUDE_DIRS})
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${YARP_LIBRARIES} ctrlLib cer_kinematics)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

