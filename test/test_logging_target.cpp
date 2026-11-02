#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "common_cpp/logging/log.hpp"

TEST(LoggingTarget, WritesWithoutRos) {
    const auto directory = std::filesystem::temp_directory_path() / "ros_common_cpp_logging_target_test";
    std::filesystem::remove_all(directory);
    ASSERT_FALSE(std::filesystem::exists(directory));

    logging::LogConfig config;
    config.logger_name = "logging_target_test";
    config.log_path = directory.string();
    config.async = false;
    logging::Logger::init_from_config(&config);

    const auto logger = logging::Logger::get_instance().get_logger();
    ASSERT_NE(logger, nullptr);
    logger->info("logging-target-test");
    logger->flush();

    bool has_log_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            has_log_file = true;
            break;
        }
    }
    EXPECT_TRUE(has_log_file);
}
