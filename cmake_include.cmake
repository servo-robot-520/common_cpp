if (NOT ROS2_WS_SOURCE_BUILD)
    find_package(common_cpp REQUIRED)
endif ()

if (ROS2_WS_SOURCE_BUILD)
    if (NOT TARGET common_cpp_core OR NOT TARGET common_cpp_logging_interface)
        message(FATAL_ERROR
                "Source workspace configuration requires in-tree common_cpp_core and "
                "common_cpp_logging_interface targets before ${PROJECT_NAME}.")
    endif ()
    target_link_libraries(${PROJECT_NAME}
            common_cpp_core
            common_cpp_logging_interface
    )
else ()
    target_link_libraries(${PROJECT_NAME}
            common_cpp::common_cpp_core
            common_cpp::common_cpp_logging_interface
    )
endif ()


# 可执行文件需要
if (BUILD_EXE)
    add_executable(${PROJECT_EXE_NAME}
            src/main.cpp
    )
    target_compile_features(${PROJECT_EXE_NAME} PRIVATE cxx_std_23)
    target_link_libraries(${PROJECT_EXE_NAME} ${PROJECT_NAME})
    ament_target_dependencies(${PROJECT_EXE_NAME} rclcpp)

    if (ROS2_WS_SOURCE_BUILD)
        if (NOT TARGET common_cpp_ros_logging)
            message(FATAL_ERROR
                    "Source workspace configuration requires the in-tree common_cpp_ros_logging "
                    "target before ${PROJECT_NAME}.")
        endif ()
        target_link_libraries(${PROJECT_EXE_NAME} common_cpp_ros_logging)
    else ()
        target_link_libraries(${PROJECT_EXE_NAME}
                common_cpp::common_cpp_ros_logging
        )
    endif ()
endif ()