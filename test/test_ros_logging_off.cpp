#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <rcl_interfaces/msg/log.hpp>
#include <rclcpp/rclcpp.hpp>

#include "common_cpp/logging/rclcpp_log_handler.hpp"
#include "common_cpp/logging/log_interface/log_macro.hpp"

namespace {

size_t count_message(const std::vector<rcl_interfaces::msg::Log>& messages,
                     const std::string& expected_message) {
    size_t count = 0;
    for (const auto& message : messages) {
        if (message.msg == expected_message) {
            ++count;
        }
    }
    return count;
}

TEST(RosLoggingOff, DropsFatalRecordsBeforeRosoutAndSpdlog) {
    const auto directory = std::filesystem::temp_directory_path() / "ros_common_cpp_ros_logging_off_test";
    std::filesystem::remove_all(directory);
    ASSERT_EQ(setenv("ROS_LOG_DIR", directory.c_str(), 1), 0);

    rclcpp::init(0, nullptr);
    rclcpp::NodeOptions options;
    options.parameter_overrides({
        rclcpp::Parameter("log.log_level", "off"),
        rclcpp::Parameter("log.log_path", directory.string()),
        rclcpp::Parameter("log.async", false),
        rclcpp::Parameter("log.logger_name", "ros_logging_off_test"),
    });
    const auto node = std::make_shared<rclcpp::Node>("ros_logging_off_test", options);

    std::mutex received_mutex;
    std::vector<rcl_interfaces::msg::Log> received_logs;
    const auto subscription = node->create_subscription<rcl_interfaces::msg::Log>(
        "/rosout", rclcpp::QoS(rclcpp::KeepLast(10)).transient_local(),
        [&received_mutex, &received_logs](const rcl_interfaces::msg::Log::SharedPtr message) {
            std::scoped_lock lock(received_mutex);
            received_logs.push_back(*message);
        });
    ASSERT_NE(subscription, nullptr);

    logging::ros::RclcppLogHandler::init_from_node(node);
    constexpr char rclcpp_marker[] = "ros-logging-off-rclcpp-fatal-marker";
    constexpr char macro_marker[] = "ros-logging-off-macro-fatal-marker";
    RCLCPP_FATAL(node->get_logger(), "%s", rclcpp_marker);
    LOG_FATAL("{}", macro_marker);

    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(node);
    for (int attempt = 0; attempt < 20; ++attempt) {
        executor.spin_some();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    {
        std::scoped_lock lock(received_mutex);
        EXPECT_EQ(count_message(received_logs, rclcpp_marker), 0U);
        EXPECT_EQ(count_message(received_logs, macro_marker), 0U);
    }

    logging::ros::RclcppLogHandler::flush();
    bool found_log_file = false;
    bool markers_absent_from_spdlog = true;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".log") {
            continue;
        }
        found_log_file = true;
        std::ifstream input(entry.path());
        const std::string content{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        markers_absent_from_spdlog = markers_absent_from_spdlog &&
                                     !content.contains(rclcpp_marker) &&
                                     !content.contains(macro_marker);
    }
    EXPECT_TRUE(found_log_file);
    EXPECT_TRUE(markers_absent_from_spdlog);
    executor.remove_node(node);
    logging::ros::RclcppLogHandler::shutdown();
    rclcpp::shutdown();
}

}  // namespace
