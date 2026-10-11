
cmake_minimum_required(VERSION 3.21)

project(snapix
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui)

add_executable(snapix main.cpp)

target_link_libraries(snapix PRIVATE
    Qt6::Core
    Qt6::Gui
)

install(TARGETS snapix
    RUNTIME DESTINATION bin
)
