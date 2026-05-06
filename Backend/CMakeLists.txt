cmake_minimum_required(VERSION 3.16)
project(testqt6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include paths
include_directories(
    ${CMAKE_SOURCE_DIR}
    D:/Qt/6.10.3/mingw_64/include
    D:/Qt/6.10.3/mingw_64/include/QtWidgets
    D:/Qt/6.10.3/mingw_64/include/QtGui
    D:/Qt/6.10.3/mingw_64/include/QtCore
    D:/Qt/6.10.3/mingw_64/include/QtNetwork
    D:/Qt/6.10.3/mingw_64/include/QtSql
    D:/Qt/6.10.3/mingw_64/include/QtHttpServer
    D:/Qt/6.10.3/mingw_64/include/QtWebSockets
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql HttpServer WebSockets)

# Find source files
file(GLOB SOURCES 
    "${CMAKE_SOURCE_DIR}/*.cpp"
    "${CMAKE_SOURCE_DIR}/net/*.cpp"
)

file(GLOB HEADERS 
    "${CMAKE_SOURCE_DIR}/*.h"
    "${CMAKE_SOURCE_DIR}/net/*.h"
)

add_executable(testqt6 WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(testqt6 PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::Network 
    Qt6::Sql 
    Qt6::HttpServer 
    Qt6::WebSockets
)

# Link with correct MinGW runtime
if(WIN32)
    set_target_properties(testqt6 PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(testqt6 PRIVATE -lmingw32 -lshell32)
endif()

# Disable entry point wrapping
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
