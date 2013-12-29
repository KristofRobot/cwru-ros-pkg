cmake_minimum_required(VERSION 2.4.6)
project(xv_11_laser_driver)
find_package(catkin REQUIRED sensor_msgs roscpp)
find_package(Boost REQUIRED COMPONENTS thread)

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES ${PROJECT_NAME}
  DEPENDS Boost
)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(include ${catkin_INCLUDE_DIRS})

add_library(${PROJECT_NAME} src/xv11_laser.cpp)
add_executable(neato_laser_publisher src/neato_laser_publisher.cpp)
target_link_libraries(neato_laser_publisher ${PROJECT_NAME} ${Boost_LIBRARIES} ${catkin_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION})
install(TARGETS neato_laser_publisher
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)
