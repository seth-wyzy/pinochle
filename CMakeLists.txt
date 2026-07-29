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

option(BUILD_PYTHON_BINDINGS "Build the Pinochle Python extension" ON)
if(BUILD_PYTHON_BINDINGS)
    find_package(pybind11 CONFIG QUIET)
    if(pybind11_FOUND)
        pybind11_add_module(pinochle_cpp
            ml/bindings/pinochle_module.cpp
            src/Pin.cpp
            src/card.cpp
            src/aiPlayer.cpp
        )
        target_include_directories(pinochle_cpp PRIVATE src)
        set_target_properties(pinochle_cpp PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
        )
    else()
        message(STATUS "pybind11 not found; Python bindings will not be built")
    endif()
endif()

enable_testing()

add_executable(card_test
    tests/card_test.cpp
    src/card.cpp
)
add_test(NAME CardTest COMMAND card_test)

add_executable(pin_test
    tests/pin_test.cpp
    src/Pin.cpp
    src/card.cpp
    src/aiPlayer.cpp
)
add_test(NAME PinTest COMMAND pin_test)
