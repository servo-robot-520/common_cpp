#include <atomic>
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
#include <rcutils/logging.h>

#include "common_cpp/logging/log_interface/log_macro.hpp"
#include "common_cpp/logging/rclcpp_log_handler.hpp"

namespace {

std::atomic_int predecessor_handler_calls{0};

void predecessor_handler(const rcutils_log_location_t*, int, const char*, rcutils_time_point_value_t,
                         const char*, va_list*) {
    ++predecessor_handler_calls;
}

size_t count_message(const std::vector<rcl_interfaces::msg::Log>& logs, const std::string& message) {
    size_t count = 0;
    for (const auto& log : logs) {
        if (log.msg == message) {
            ++count;
        }
    }
    return count;
}

TEST(RosLoggingTarget, RoutesRclcppAndMacroLogsToRosoutAndSpdlogWithoutPredecessorHandler) {
    ASSERT_FALSE(rclcpp::ok());
    const auto log_directory = std::filesystem::temp_directory_path() / "ros_common_cpp_ros_logging_test";
    std::filesystem::remove_all(log_directory);
    ASSERT_EQ(setenv("ROS_LOG_DIR", log_directory.c_str(), 1), 0);
    rclcpp::init(0, nullptr);

    rclcpp::NodeOptions options;
    options.parameter_overrides({
        rclcpp::Parameter("log.log_level", "debug"),
        rclcpp::Parameter("log.log_path", log_directory.string()),
        rclcpp::Parameter("log.async", true),
        rclcpp::Parameter("log.logger_name", "ros_logging_target_test"),
    });
    const auto node = std::make_shared<rclcpp::Node>("ros_logging_target_test", options);

    // A node may already own the log parameters. init_from_node() must reuse
    // them instead of attempting a second declaration.
    node->declare_parameter<std::string>("log.log_level", "info");
    node->declare_parameter<std::string>("log.log_path", "./ignored");
    node->declare_parameter<bool>("log.async", true);
    node->declare_parameter<std::string>("log.logger_name", "ignored");
    node->declare_parameter<std::vector<std::string>>("log.filter.black", std::vector<std::string>{});

    std::mutex received_mutex;
    std::vector<rcl_interfaces::msg::Log> received_logs;
    const auto subscription = node->create_subscription<rcl_interfaces::msg::Log>(
        "/rosout", rclcpp::QoS(rclcpp::KeepLast(1000)).transient_local(),
        [&received_mutex, &received_logs](const rcl_interfaces::msg::Log::SharedPtr message) {
            std::scoped_lock lock(received_mutex);
            received_logs.push_back(*message);
        });
    ASSERT_NE(subscription, nullptr);

    predecessor_handler_calls = 0;
    rcutils_logging_set_output_handler(&predecessor_handler);
    EXPECT_NO_THROW(logging::ros::RclcppLogHandler::init_from_node(node));

    constexpr char rclcpp_message[] = "rclcpp-bridge-message";
    constexpr char macro_message[] = "macro-bridge-message";
    RCLCPP_INFO(node->get_logger(), "%s", rclcpp_message);
    LOG_INFO("{}", macro_message);

    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(node);
    for (int attempt = 0; attempt < 100; ++attempt) {
        executor.spin_some();
        {
            std::scoped_lock lock(received_mutex);
            if (count_message(received_logs, rclcpp_message) == 1U &&
                count_message(received_logs, macro_message) == 1U) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::scoped_lock lock(received_mutex);
        // This is the regression check for the non-chained handler: neither
        // source can produce a second /rosout record through the predecessor.
        EXPECT_EQ(count_message(received_logs, rclcpp_message), 1U);
        EXPECT_EQ(count_message(received_logs, macro_message), 1U);
    }

    // The ROS logging handler is process-global and can be called from executor
    // threads as well as application worker threads. Exercise both entry paths
    // concurrently; the test's unique marker must survive in the spdlog file.
    constexpr int thread_count = 4;
    constexpr int records_per_thread = 24;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        workers.emplace_back([node, thread_index] {
            for (int record_index = 0; record_index < records_per_thread; ++record_index) {
                if (record_index % 2 == 0) {
                    RCLCPP_INFO(node->get_logger(), "rclcpp-thread-%d-record-%d", thread_index, record_index);
                } else {
                    LOG_INFO("macro-thread-{}-record-{}", thread_index, record_index);
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(predecessor_handler_calls.load(), 0);
    logging::ros::RclcppLogHandler::flush();

    bool found_log_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(log_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            const auto content = [&entry] {
                std::ifstream input(entry.path());
                return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            }();
            found_log_file = content.contains(rclcpp_message) && content.contains(macro_message) &&
                             content.contains("test_ros_logging.cpp");
            for (int thread_index = 0; found_log_file && thread_index < thread_count; ++thread_index) {
                found_log_file = content.contains(
                    "rclcpp-thread-" + std::to_string(thread_index) + "-record-0") &&
                    content.contains("macro-thread-" + std::to_string(thread_index) + "-record-1");
            }
            if (found_log_file) {
                break;
            }
        }
    }
    EXPECT_TRUE(found_log_file);

    executor.remove_node(node);
    logging::ros::RclcppLogHandler::shutdown();
    rclcpp::shutdown();
}

}  // namespace
