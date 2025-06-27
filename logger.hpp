#ifndef LOGGER_HPP
#define LOGGER_HPP
/*
    LogLevel Usage in Game Engine Context:

    LogLevel::Trace
    - Extremely detailed, fine-grained logs.
    - Used for tracing code execution paths, function entry/exit, memory ops,
   ECS steps, etc.
    - Typically disabled in production due to volume/performance cost.

    LogLevel::Debug
    - General-purpose debugging information.
    - Used to track game state, system behavior, asset loading, AI decisions,
   input handling, etc.
    - Useful during active development; not enabled in production builds.

    LogLevel::Info
    - High-level runtime information indicating normal operation.
    - Examples: level loaded, player joined, shaders compiled, assets
   hot-reloaded.
    - Suitable for both development and production for visibility into system
   flow.

    LogLevel::Warn
    - Indicates a potential issue or unexpected condition that is non-fatal.
    - Examples: missing optional assets, deprecated API use, high frame times,
   fallback behavior.
    - Deserves attention, useful for QA, testing, and sometimes user-visible
   logs.

    LogLevel::Error
    - A serious issue that impacted functionality but did not crash the engine.
    - Examples: asset load failures, script exceptions, physics errors, network
   disconnects.
    - Should be logged prominently and often triggers error-handling logic.

    LogLevel::Critical
    - Fatal errors that prevent continued execution or indicate
   corruption/system failure.
    - Examples: GPU device failure, save data corruption, out-of-memory, engine
   init failure.
    - Typically results in shutdown, crash, or forced failover.
*/

#include <fmt/core.h>
#include <string_view>
#include <iostream>
#include <bitset>

// -------------------- LogLevel Definition --------------------
enum class LogLevel { Trace = 0, Debug, Info, Warn, Error, Critical, Count };

class ILogger {
  public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, std::string_view message) = 0;

    void enable_level(LogLevel level) { enabled_levels.set(static_cast<size_t>(level), true); }

    void disable_level(LogLevel level) { enabled_levels.set(static_cast<size_t>(level), false); }

    void enable_all_levels() { enabled_levels.set(); }

    void disable_all_levels() { enabled_levels.reset(); }

    bool is_level_enabled(LogLevel level) const { return enabled_levels.test(static_cast<size_t>(level)); }

    void set_name(std::string logger_name) { name = std::move(logger_name); }
    const std::string &get_name() const { return name; }

    template <typename... Args> void trace(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Trace))
            log(LogLevel::Trace, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    template <typename... Args> void debug(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Debug))
            log(LogLevel::Debug, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    template <typename... Args> void info(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Info))
            log(LogLevel::Info, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    template <typename... Args> void warn(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Warn))
            log(LogLevel::Warn, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    template <typename... Args> void error(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Error))
            log(LogLevel::Error, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

    template <typename... Args> void critical(fmt::format_string<Args...> fmt_str, Args &&...args) {
        if (is_level_enabled(LogLevel::Critical))
            log(LogLevel::Critical, fmt::format(fmt_str, std::forward<Args>(args)...));
    }

  protected:
    std::bitset<static_cast<size_t>(LogLevel::Count)> enabled_levels{0b111111}; // All enabled by default
    std::string name;                                                           // Optional logger name
};

// -------------------- ConsoleLogger Implementation --------------------
class ConsoleLogger : public ILogger {
  public:
    explicit ConsoleLogger(std::string_view logger_name = "") { set_name(std::string(logger_name)); }

    void log(LogLevel level, std::string_view message) override {
        if (!name.empty()) {
            std::cout << "[" << name << "] ";
        }
        std::cout << "[" << level_to_string(level) << "] " << message << std::endl;
    }

  private:
    const char *level_to_string(LogLevel level) {
        switch (level) {
        case LogLevel::Trace:
            return "Trace";
        case LogLevel::Debug:
            return "Debug";
        case LogLevel::Info:
            return "Info";
        case LogLevel::Warn:
            return "Warn";
        case LogLevel::Error:
            return "Error";
        case LogLevel::Critical:
            return "Critical";
        default:
            return "Unknown";
        }
    }
};

#include <chrono>

class RateLimitedConsoleLogger : public ConsoleLogger {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::duration<double>;

    explicit RateLimitedConsoleLogger(double max_frequency_hz)
        : min_interval(1.0 / max_frequency_hz), last_tick_time(clock::now()) {}

    void tick() {
        time_point now = clock::now();
        duration elapsed = now - last_tick_time;

        if (elapsed >= min_interval) {
            enable_all_levels();
            last_tick_time = now;
        } else {
            disable_all_levels();
        }
    }

  private:
    duration min_interval;
    time_point last_tick_time;
};

#endif
