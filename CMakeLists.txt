cmake_minimum_required(VERSION 3.10)
project(pin)

set(CMAKE_CXX_STANDARD 17)

# Optional: add include directories if headers are in a different directory
include_directories(include)

# List all source and header files (optional to list headers)
add_executable(pin
    src/main.cpp 
    src/Pin.cpp
    src/card.cpp
    src/aiPlayer.cpp
)


target_link_libraries(pin)
