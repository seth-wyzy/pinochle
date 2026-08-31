cmake_minimum_required(VERSION 3.10)
project(pin)

set(CMAKE_CXX_STANDARD 17)

# A default Debug build makes the simulator benchmark misleadingly slow.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
    # Prefer the pybind11 package belonging to the Python used for training.
    find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
    if(Python3_Interpreter_FOUND AND Python3_Development.Module_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
            OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE PYBIND11_CMAKE_RESULT
            ERROR_QUIET
        )
        if(PYBIND11_CMAKE_RESULT EQUAL 0)
            find_package(pybind11 CONFIG QUIET PATHS "${PYBIND11_CMAKE_DIR}")
        endif()
    endif()
    if(NOT pybind11_FOUND)
        find_package(pybind11 CONFIG QUIET)
    endif()
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
