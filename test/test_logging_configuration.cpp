#include <gtest/gtest.h>

#include <stdexcept>

#include "ros_common_cpp/logging/log.hpp"

TEST(LoggingConfiguration, RejectsConfigurationAfterFallbackLoggerUse) {
    const auto logger = logging::Logger::get_instance().get_logger();
    ASSERT_NE(logger, nullptr);

    logging::LogConfig config;
    config.logger_name = "should_not_replace_default";
    EXPECT_THROW(logging::Logger::init_from_config(&config), std::logic_error);
}
