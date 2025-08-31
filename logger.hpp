#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fmt/core.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <map>
#include <string_view>

const std::map<spdlog::level::level_enum, std::string> level_to_string = {
    {spdlog::level::trace, "trace"}, {spdlog::level::debug, "debug"}, {spdlog::level::info, "info"},
    {spdlog::level::warn, "warn"},   {spdlog::level::err, "err"},     {spdlog::level::critical, "critical"},
    {spdlog::level::off, "off"}};

// NOTE: this is replacing everything else.
class Logger {
  public:
    explicit Logger(std::string_view logger_name = "section_logger")
        : section_depth_(0), current_level_(spdlog::level::debug), current_pattern_("[%H:%M:%S.%f] [%^%l%$] %v") {
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        logger_ = std::make_shared<spdlog::logger>(std::string(logger_name), stdout_sink);
        spdlog::register_logger(logger_);
        configure(current_level_, current_pattern_);
    }

    void add_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
        logger_->sinks().push_back(sink);
        reapply_formatting(); // <-- ensure new sink matches
    }

    void add_file_sink(const std::string &file_path, bool truncate = false) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, truncate);
        add_sink(file_sink);
    }

    void add_rotating_file_sink(const std::string &file_path, size_t max_size, size_t max_files) {
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, max_size, max_files);
        add_sink(rotating_sink);
    }

    void add_stdout_sink(bool color = true) {
        if (color) {
            add_sink(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        } else {
            add_sink(std::make_shared<spdlog::sinks::stdout_sink_mt>());
        }
    }

    void configure(spdlog::level::level_enum lvl, std::string_view pattern) {
        current_level_ = lvl;
        current_pattern_ = std::string(pattern);
        reapply_formatting();
    }

    template <typename... Args>
    void log(spdlog::level::level_enum lvl, fmt::format_string<Args...> fmt_str, Args &&...args) {
        auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);

        // Compute the maximum length of all level names
        static size_t max_level_len = 0;
        if (max_level_len == 0) {
            for (const auto &p : level_to_string) {
                if (p.second.size() > max_level_len) {
                    max_level_len = p.second.size();
                }
            }
        }

        // Prepend section bars
        std::string prefix;
        for (int i = 0; i < section_depth_; ++i) {
            prefix += "| ";
        }

        // Pad spaces after level name
        const std::string &level_str = level_to_string.at(lvl);
        size_t padding = max_level_len - level_str.size();

        // Combine everything
        msg = std::string(padding, ' ') + prefix + msg;

        logger_->log(lvl, msg);
    }

    // Convenience wrappers
    template <typename... Args> void trace(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::trace, fmt_str, std::forward<Args>(args)...);
    }
    template <typename... Args> void debug(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::debug, fmt_str, std::forward<Args>(args)...);
    }
    template <typename... Args> void info(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::info, fmt_str, std::forward<Args>(args)...);
    }
    template <typename... Args> void warn(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::warn, fmt_str, std::forward<Args>(args)...);
    }
    template <typename... Args> void error(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::err, fmt_str, std::forward<Args>(args)...);
    }
    template <typename... Args> void critical(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::critical, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args> void start_section(fmt::format_string<Args...> fmt_str, Args &&...args) {
        start_section(spdlog::level::info, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void start_section(spdlog::level::level_enum lvl, fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(lvl, "=== start {} === {{", fmt::format(fmt_str, std::forward<Args>(args)...));
        ++section_depth_;
    }

    template <typename... Args> void end_section(fmt::format_string<Args...> fmt_str, Args &&...args) {
        end_section(spdlog::level::info, fmt_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void end_section(spdlog::level::level_enum lvl, fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (section_depth_ > 0)
            --section_depth_;
        log(lvl, "===   end {} === }}", fmt::format(fmt_str, std::forward<Args>(args)...));
    }

  private:
    void reapply_formatting() {
        logger_->set_level(current_level_);
        for (auto &sink : logger_->sinks()) {
            sink->set_pattern(current_pattern_);
        }
    }

    int section_depth_;
    std::shared_ptr<spdlog::logger> logger_;
    spdlog::level::level_enum current_level_;
    std::string current_pattern_;
};

class LogSection {
  public:
    template <typename... Args>
    LogSection(Logger &logger, fmt::format_string<Args...> fmt_str, Args &&...args)
        : logger_(logger), section_name_(fmt::format(fmt_str, std::forward<Args>(args)...)) {
        logger_.start_section("{}", section_name_);
    }

    ~LogSection() { logger_.end_section("{}", section_name_); }

    LogSection(const LogSection &) = delete;
    LogSection &operator=(const LogSection &) = delete;

  private:
    Logger &logger_;
    std::string section_name_; // store formatted name here
};

#endif
