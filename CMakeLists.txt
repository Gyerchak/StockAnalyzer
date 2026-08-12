cmake_minimum_required(VERSION 3.20)
project(StockAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)

add_executable(stockanalyzer
  src/main.cpp
  src/api.cpp
  src/api.hpp
  src/watchlist.cpp
  src/watchlist.hpp
)

target_include_directories(stockanalyzer PRIVATE /usr/include)
target_link_libraries(stockanalyzer PRIVATE CURL::libcurl)
