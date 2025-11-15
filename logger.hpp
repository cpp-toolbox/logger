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
#include <sstream>

const std::map<spdlog::level::level_enum, std::string> level_to_string = {
    {spdlog::level::trace, "trace"},  {spdlog::level::debug, "debug"}, {spdlog::level::info, "info"},
    {spdlog::level::warn, "warning"}, {spdlog::level::err, "error"},   {spdlog::level::critical, "critical"},
    {spdlog::level::off, "off"}};

// NOTE: this is replacing everything else.
class Logger {
  public:
    explicit Logger(std::string_view base_name = "section_logger")
        : section_depth_(0), current_level_(spdlog::level::debug), current_pattern_("[%H:%M:%S.%f] [%^%l%$] %v") {
        std::string logger_name = std::string(base_name);

        // If logger exists, increment postfix until unique
        int counter = 1;
        while (spdlog::get(logger_name) != nullptr) {
            logger_name = std::string(base_name) + "_" + std::to_string(counter++);
        }

        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        logger_ = std::make_shared<spdlog::logger>(logger_name, stdout_sink);
        spdlog::register_logger(logger_);

        configure(current_level_, current_pattern_);
    }

    spdlog::level::level_enum get_current_level() const { return current_level_; }

    void set_level(spdlog::level::level_enum lvl) {
        current_level_ = lvl;
        logger_->set_level(lvl);
    }

    void remove_all_sinks() { logger_->sinks().clear(); }

    void add_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
        logger_->sinks().push_back(sink);
        reapply_formatting(); // <-- ensure new sink matches
    }

    void add_file_sink(const std::string &file_path, bool overwrite_file = true) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, overwrite_file);
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

    // ====== Logging core ======
    // Format-string overload (compile-time checked)
    template <typename... Args>
    void log(spdlog::level::level_enum lvl, fmt::format_string<Args...> fmt_str, Args &&...args) {
        format_and_log(lvl, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    // Direct runtime string overload
    void log(spdlog::level::level_enum lvl, std::string_view msg) { format_and_log(lvl, std::string(msg)); }

    // ====== Convenience wrappers ======
    template <typename... Args> void trace(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::trace, fmt_str, std::forward<Args>(args)...);
    }
    void trace(std::string_view msg) { log(spdlog::level::trace, msg); }

    template <typename... Args> void debug(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::debug, fmt_str, std::forward<Args>(args)...);
    }
    void debug(std::string_view msg) { log(spdlog::level::debug, msg); }

    template <typename... Args> void info(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::info, fmt_str, std::forward<Args>(args)...);
    }
    void info(std::string_view msg) { log(spdlog::level::info, msg); }

    template <typename... Args> void warn(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::warn, fmt_str, std::forward<Args>(args)...);
    }
    void warn(std::string_view msg) { log(spdlog::level::warn, msg); }

    template <typename... Args> void error(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::err, fmt_str, std::forward<Args>(args)...);
    }
    void error(std::string_view msg) { log(spdlog::level::err, msg); }

    template <typename... Args> void critical(fmt::format_string<Args...> fmt_str, Args &&...args) {
        log(spdlog::level::critical, fmt_str, std::forward<Args>(args)...);
    }
    void critical(std::string_view msg) { log(spdlog::level::critical, msg); }

    // Sections still require formatting, but you could add overloads too if you want.
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

    void disable_all_levels() {
        current_level_ = spdlog::level::off;
        logger_->set_level(current_level_);
    }

  private:
    void format_and_log(spdlog::level::level_enum lvl, std::string msg) {
        // Compute the maximum length of all level names
        static size_t max_level_len = [] {
            size_t max_len = 0;
            for (const auto &p : level_to_string) {
                if (p.second.size() > max_len) {
                    max_len = p.second.size();
                }
            }
            return max_len;
        }();

        // Precompute the level padding
        const std::string &level_str = level_to_string.at(lvl);
        size_t padding = max_level_len - level_str.size();

        // Prepend section bars
        std::string prefix;
        for (int i = 0; i < section_depth_; ++i) {
            prefix += "| ";
        }

        // Now split on newlines
        std::istringstream iss(msg);
        std::string line;
        while (std::getline(iss, line)) {
            std::string full_msg = std::string(padding, ' ') + prefix + line;
            logger_->log(lvl, full_msg);
        }
    }

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

/**
 * @class LogSection
 * @brief RAII helper that automatically creates and closes a structured log section, with an optional mode to
 * temporarily disable all logging within its scope.
 *
 * When constructed, this class begins a new log section via `Logger::start_section()`. When it goes out of scope, it
 * ends the section automatically.
 *
 * If constructed with `disable_logging = true`, it will also temporarily disable all logging levels for the given
 * `Logger` while the section is active, restoring the previous level upon destruction.
 *
 * The purpose of being able to disable the logger is that you don't have to remove all of the logging statements you
 * previously wrote down, the benefit being that if you ever need to re-debug or analyze the code again you still have
 * an easy way of re-enabling that instead of having to re-write all the logging statements again
 *
 * This makes it possible to both group logs and silence verbose internal sections
 * using the same class.
 *
 * Example usage:
 * @code
 * {
 *     // Normal section
 *     LogSection section(global_logger, "Gameplay Update");
 *     global_logger.info("running physics...");
 *
 *     // Muted section
 *     {
 *         LogSection muted_section(global_logger, "Pathfinding Update", true);
 *         global_logger.info("this will NOT be logged");
 *     }
 *
 *     global_logger.info("logging restored");
 * } // Sections automatically end here
 * @endcode
 */
class LogSection {
  public:
    /**
     * @brief Constructs a new log section.
     *
     * Starts a new logging section and optionally disables all logging output
     * for the given logger while this object exists.
     *
     * @param logger Reference to the logger instance managing this section.
     * @param section_name The name of the section.
     * @param logging_enabled Whether logging is enabled for this section, when false it will temporarily disable
     * logging in this section.
     *
     * @note we can't use variadic args because we want a default argument for disable logging at the end.
     */
    LogSection(Logger &logger, const std::string &section_name, bool logging_enabled = true)
        : logger_(logger), section_name_(section_name), disable_logging_(not logging_enabled),
          previous_level_(logger.get_current_level()) {

        if (disable_logging_) {
            logger_.disable_all_levels();
        } else {
            logger_.start_section("{}", section_name_);
        }
    }

    /**
     * @brief Destructor that automatically ends the log section.
     *
     * If logging was disabled during construction, the previous log level
     * is restored before ending the section.
     */
    ~LogSection() {
        if (disable_logging_) {
            logger_.set_level(previous_level_);
        } else {
            logger_.end_section("{}", section_name_);
        }
    }

    /// Deleted copy constructor to prevent accidental copies.
    LogSection(const LogSection &) = delete;

    /// Deleted copy assignment operator to prevent accidental assignments.
    LogSection &operator=(const LogSection &) = delete;

  private:
    Logger &logger_;                           ///< Reference to the logger that handles this section.
    std::string section_name_;                 ///< The section name used in log output.
    bool disable_logging_;                     ///< Whether logging was temporarily disabled for this section.
    spdlog::level::level_enum previous_level_; ///< The log level to restore after destruction.
};

extern Logger global_logger;

#endif
