cmake_minimum_required(VERSION 3.16)

# Wokwi simulation flag (set via CI or manually)
if(WOKWI_SIMULATION)
    add_compile_definitions(WOKWI_SIMULATION=1)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fresnel-beacon)
