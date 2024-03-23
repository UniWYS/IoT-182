# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{RMAKER_PATH})
  set(RMAKER_PATH $ENV{RMAKER_PATH})
else()
  set(RMAKER_PATH ${CMAKE_CURRENT_LIST_DIR}/../..)
endif(DEFINED ENV{RMAKER_PATH})

# Add RainMaker components and other common application components
set(EXTRA_COMPONENT_DIRS D:/rainmaker/esp-rainmaker/components/esp-insights/components D:/rainmaker/esp-rainmaker/components D:/rainmaker/esp-rainmaker/examples/common D:/rain_mker/switch/components)

set(PROJECT_VER "1.1")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(switch)
