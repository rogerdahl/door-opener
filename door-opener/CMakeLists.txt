# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(ENV{IDF_PATH} $ENV{HOME}/sdk/esp-idf)

# The path to the Python venv is printed when activating ESP-IDF.
set(ENV{IDF_PYTHON_ENV_PATH} $ENV{HOME}/.espressif/python_env/idf4.3_py3.9_env/bin/python)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(door-opener)

# Colors in build output
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)

set(
  CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wno-unused-function"
)

#CFLAGS += -std=c99 -pedantic -pedantic-errors -Werror -g0 -Os \
#-fno-strict-overflow -fno-strict-aliasing \
#-pthread \
#-Wno-unused-label \
#-Wno-unused-function \
#$(INC)
#SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
#
