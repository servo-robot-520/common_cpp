//
// Created by greenhand520 on 2026/5/5.
//

#pragma once

#include <string>
#include <vector>
#include <spdlog/sinks/base_sink.h>

namespace logging {

    struct FilterRule {
        enum Type { Start,
                    End,
                    Include,
                    Equal };
        Type type;
        std::string pattern;
    };

    /// 自定义 spdlog sink：根据黑名单规则过滤日志消息，命中的日志被丢弃，未命中的转发给底层 sinks。
    template <typename Mutex>
    class FilterSink final : public spdlog::sinks::base_sink<Mutex> {
    public:
        explicit FilterSink(std::vector<spdlog::sink_ptr> inner_sinks) : inner_sinks_(std::move(inner_sinks)) {
        }

        void add_rule(const FilterRule::Type type, const std::string& pattern) {
            rules_.push_back({.type = type, .pattern = pattern});
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            const std::string payload(msg.payload.begin(), msg.payload.end());
            for (const auto& rule : rules_) {
                if (matches(payload, rule))
                    return;
            }
            for (const auto& sink : inner_sinks_) {
                sink->log(msg);
            }
        }

        void flush_() override {
            for (const auto& sink : inner_sinks_) {
                sink->flush();
            }
        }

    private:
        static bool matches(const std::string& payload, const FilterRule& rule) {
            switch (rule.type) {
            case FilterRule::Start:
                return payload.rfind(rule.pattern, 0) == 0;
            case FilterRule::End:
                return payload.size() >= rule.pattern.size() &&
                    payload.compare(payload.size() - rule.pattern.size(),
                                    rule.pattern.size(), rule.pattern) == 0;
            case FilterRule::Include:
                return payload.find(rule.pattern) != std::string::npos;
            case FilterRule::Equal:
                return payload == rule.pattern;
            }
            return false;
        }

        std::vector<FilterRule> rules_;
        std::vector<spdlog::sink_ptr> inner_sinks_;
    };

} // namespace common_log
