cmake_minimum_required(VERSION 2.8.12)

project(swarm_mission)

find_package(Threads REQUIRED)

if(NOT MSVC)
    add_definitions("-std=c++11 -Wall -Wextra -Werror")
else()
    add_definitions("-std=c++11 -WX -W2")
endif()

find_package(MAVSDK REQUIRED)

add_executable(swarm_mission
    swarm_mission.cpp
    utils.cpp
)

target_link_libraries(swarm_mission
    MAVSDK::mavsdk_action
    MAVSDK::mavsdk_mission
    MAVSDK::mavsdk_telemetry
    MAVSDK::mavsdk
    ${CMAKE_THREAD_LIBS_INIT}
)
