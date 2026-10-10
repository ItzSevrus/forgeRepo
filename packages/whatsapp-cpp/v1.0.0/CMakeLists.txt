
cmake_minimum_required(VERSION 3.16)

project(WhatsAppCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
)

add_executable(whatsapp
    main.cpp
    resources.qrc
)

target_link_libraries(whatsapp PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
)
