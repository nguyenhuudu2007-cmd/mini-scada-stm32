cmake_minimum_required(VERSION 3.16)
project(ServoHMI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tự động sinh mã MOC cho Signals & Slots của Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(CTest)

# Tự động tìm Qt6, nếu không có sẽ dùng Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Test)

add_library(ServoProtocol STATIC
    servo_protocol.h
    servo_protocol.cpp
)

target_link_libraries(ServoProtocol PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(ServoProtocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(ServoProtocol PRIVATE -Wall -Wextra)

add_executable(ServoHMI
    main.cpp
    main_window.h
    main_window.cpp
    slider_page.h
    slider_page.cpp
    sweep_page.h
    sweep_page.cpp
    hardware_test_page.h
    hardware_test_page.cpp
    log_page.h
    log_page.cpp
)

target_link_libraries(ServoHMI PRIVATE
    ServoProtocol
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

target_compile_options(ServoHMI PRIVATE -Wall -Wextra)

if(BUILD_TESTING)
    add_executable(test_servo_protocol
        tests/test_servo_protocol.cpp
    )

    target_link_libraries(test_servo_protocol PRIVATE
        ServoProtocol
        Qt${QT_VERSION_MAJOR}::Test
    )

    target_compile_options(test_servo_protocol PRIVATE -Wall -Wextra)

    add_test(NAME ServoProtocolTests COMMAND test_servo_protocol)
endif()
