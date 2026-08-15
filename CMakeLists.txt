cmake_minimum_required(VERSION 3.22)
set(PROJECT_VERSION 2.0.0)
project(common_cpp VERSION ${PROJECT_VERSION})

option(BUILD_EXE "Build node executable file" ON)
option(BUILD_TESTING OFF)

# The package exports static libraries which can be linked into ROS 2 shared
# components. Compile all static-library objects as PIC so those consumers can
# link them without relocation errors.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT SKIP_CXX23_STDLIB_CHECK AND NOT _CXX23_STDLIB_VERIFIED)
    set(_saved_flags "${CMAKE_REQUIRED_FLAGS}")
    string(APPEND CMAKE_REQUIRED_FLAGS " -std=c++23")

    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_QUIET TRUE)

    string(TOUPPER "${PROJECT_NAME}" _PRJ_UPPER)
    check_cxx_source_compiles([[
        #include <expected>
        #include <format>
        #include <ranges>
        int main() {
            std::expected<int, int> result{42};
            const auto text = std::format("{}", result.value());
            const auto values = std::views::iota(0, 1);
            return static_cast<int>(text.size() + *values.begin());
        }
    ]] ${_PRJ_UPPER}_HAS_REQUIRED_CXX23_STDLIB)

    if (NOT ${_PRJ_UPPER}_HAS_REQUIRED_CXX23_STDLIB)
        message(FATAL_ERROR
                "${PROJECT_NAME} requires a C++23 standard library with <expected>, "
                "<format>, and ranges support. Use GCC 13+ or an equivalent compiler.")
    endif ()
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(rcl REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(rcutils REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(spdlog REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(GPIOD REQUIRED libgpiod)
else()
    find_library(GPIOD_LIBRARIES NAMES gpiod)
    find_path(GPIOD_INCLUDE_DIRS gpiod.h)
endif()

function(common_cpp_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${target} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -std=c++2b)
    endif ()
endfunction()


##### common_cpp_core #####
add_library(common_cpp_core STATIC
        include/common_cpp/BS_thread_pool.hpp
        include/common_cpp/utils/common_utils.hpp
        include/common_cpp/utils/file_utils.hpp
        include/common_cpp/utils/ros_utils.hpp
        src/utils/common_utils.cpp
        src/utils/file_utils.cpp
        src/utils/ros_utils.cpp
)
common_cpp_configure_target(common_cpp_core)
ament_target_dependencies(common_cpp_core
        ament_index_cpp
        rclcpp
)

##### common_cpp_action #####
add_library(common_cpp_action STATIC
        include/common_cpp/utils/action_utils.hpp
        src/utils/action_utils.cpp
)
common_cpp_configure_target(common_cpp_action)
target_link_libraries(common_cpp_action
        common_cpp_core
)
ament_target_dependencies(common_cpp_action
        rclcpp_action
)

##### common_cpp_logging_interface #####
add_library(common_cpp_logging_interface INTERFACE
        include/common_cpp/logging/log_interface/log_interface.hpp
        include/common_cpp/logging/log_interface/log_manager.hpp
        include/common_cpp/logging/log_interface/log_macro.hpp
)
target_include_directories(common_cpp_logging_interface INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(common_cpp_logging_interface INTERFACE
        fmt::fmt
)

##### common_cpp_logging #####
# Generic spdlog backend. It must remain usable by ordinary C++ programs.
add_library(common_cpp_logging STATIC
        include/common_cpp/logging/filter_sink.hpp
        include/common_cpp/logging/log.hpp
        src/logging/log.cpp
)
common_cpp_configure_target(common_cpp_logging)
target_link_libraries(common_cpp_logging
        common_cpp_logging_interface
        spdlog::spdlog
        Threads::Threads
)

##### common_cpp_ros_logging #####
# ROS/rcutils 到 spdlog 的桥接器。该 target 故意不导入 ros_common_interface 和其他日志有关的目标。
add_library(common_cpp_ros_logging STATIC
        include/common_cpp/logging/rclcpp_log_handler.hpp
        include/common_cpp/logging/ros_log_adapter.hpp
        src/logging/rclcpp_log_handler.cpp
        src/logging/ros_log_adapter.cpp
)
common_cpp_configure_target(common_cpp_ros_logging)
target_link_libraries(common_cpp_ros_logging common_cpp_logging)
ament_target_dependencies(common_cpp_ros_logging rcl rclcpp rcutils)

##### common_cpp_host_hw #####
add_library(common_cpp_host_hw STATIC
        include/common_cpp/host_hw/gpio_pin.hpp
        include/common_cpp/host_hw/serial_port.hpp
        src/host_hw/gpio_pin.cpp
        src/host_hw/serial_port.cpp
)
common_cpp_configure_target(common_cpp_host_hw)
target_link_libraries(common_cpp_host_hw ${GPIOD_LIBRARIES})

##### common_cpp #####
# 不推荐使用这个target，除非明确需要所有依赖target
add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME} INTERFACE
        common_cpp_core
        common_cpp_action
        common_cpp_logging_interface
        common_cpp_logging
        common_cpp_ros_logging
)
target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if (BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_logging_target test/test_logging_target.cpp)
    target_link_libraries(test_logging_target common_cpp_logging)

    ament_add_gtest(test_logging_configuration test/test_logging_configuration.cpp)
    target_link_libraries(test_logging_configuration common_cpp_logging)

    ament_add_gtest(test_ros_logging test/test_ros_logging.cpp)
    target_link_libraries(test_ros_logging common_cpp_ros_logging)
    ament_target_dependencies(test_ros_logging rclcpp rcl_interfaces rcutils)

    ament_add_gtest(test_ros_logging_off test/test_ros_logging_off.cpp)
    target_link_libraries(test_ros_logging_off common_cpp_ros_logging)
    ament_target_dependencies(test_ros_logging_off rclcpp rcl_interfaces)

endif ()

set(common_cpp_TARGETS
        common_cpp_core
        common_cpp_action
        common_cpp_logging_interface
        common_cpp_logging
        common_cpp_ros_logging
        common_cpp_host_hw
        ${PROJECT_NAME}
)

install(TARGETS ${common_cpp_TARGETS}
        EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(DIRECTORY include/
        DESTINATION include
)

ament_export_include_directories(include)
ament_export_dependencies(
        ament_index_cpp
        fmt
        rcl
        rclcpp
        rclcpp_action
        rcl_interfaces
        rcutils
        spdlog
        std_msgs
        Threads
)
ament_export_targets(${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_package()
