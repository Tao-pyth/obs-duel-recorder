cmake_minimum_required(VERSION 3.24)

project(obs-duel-recorder-plugin VERSION 0.4.0 LANGUAGES CXX)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

add_library(obs-duel-recorder MODULE
  src/plugin-main.cpp
  src/worker-api.cpp
  src/worker-launcher.cpp
)

target_compile_features(obs-duel-recorder PRIVATE cxx_std_17)

target_link_libraries(obs-duel-recorder
  PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
)

if(WIN32)
  target_link_libraries(obs-duel-recorder PRIVATE winhttp)
endif()

set_target_properties(obs-duel-recorder PROPERTIES
  PREFIX ""
  OUTPUT_NAME "obs-duel-recorder"
)
