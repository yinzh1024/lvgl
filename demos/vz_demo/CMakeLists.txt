cmake_minimum_required(VERSION 3.12.4)
#########################################################################
SET(BUILD_PROJECT_NAME lvgl_demo)

#add_definitions(
#  -fsanitize=leak
#  -fno-omit-frame-pointer
#  -static-libasan
#)

INCLUDE_DIRECTORIES(.)

SET(DEMO_SRC
  main.cpp
)

ADD_EXECUTABLE(${BUILD_PROJECT_NAME}
  ${DEMO_SRC}
)

TARGET_LINK_LIBRARIES(${BUILD_PROJECT_NAME}
  lvgl
  dev_sdk
  jsoncpp
  ${VZPROJECT_LINK_LIB}
  ${VZ_DRV_LINK_LIB}
  -lfreetype
  -lpthread
#  liblsan.a
)

INSTALL(TARGETS ${BUILD_PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_RUNTIME_INSTALL_DIRECTORY}
  LIBRARY DESTINATION ${CMAKE_LIBRARY_INSTALL_DIRECTORY}
  ARCHIVE DESTINATION ${CMAKE_ARCHIVE_INSTALL_DIRECTORY}
)
