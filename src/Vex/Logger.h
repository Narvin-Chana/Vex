#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <string>
#include <string_view>

#include <Vex/Platform/Debug.h>
#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>

namespace vex
{

// Format the current system time
std::string GetTimestampString();

enum LogLevel : u8
{
    Verbose,
    Info,
    Warning,
    Error,
    Fatal
};

constexpr inline std::string_view LogLevelToString(LogLevel logLevel)
{
    switch (logLevel)
    {
    case Verbose:
        return "Verbose";
    case Info:
        return "Info";
    case Warning:
        return "Warning";
    case Error:
        return "Error";
    case Fatal:
        return "Fatal";
    default:
        return "Invalid";
    }
}

/* clang-format off */
BEGIN_VEX_ENUM_FLAGS(LogDestination, u8)
    None = 0, 
    Console = 1 << 0, 
    File = 1 << 1,
END_VEX_ENUM_FLAGS();
/* clang-format on */

struct Logger
{
    Logger();
    ~Logger();

    template <class... Args>
    void Log(LogLevel level, std::format_string<Args...> formatMessage, Args&&... args)
    {
        std::string timeStampedFormatMessage = std::format("[{}][{}] {}",
                                                           GetTimestampString(),
                                                           LogLevelToString(level),
                                                           std::format(formatMessage, std::forward<Args>(args)...));

        if (destinationFlags & LogDestination::Console)
        {
            std::println("{}", timeStampedFormatMessage);
            std::flush(std::cout);
        }

        if (destinationFlags & LogDestination::File)
        {
            if (!logOutput)
            {
                OpenLogFile();
            }
            *logOutput << timeStampedFormatMessage << '\n';
            // Frequent flush means that even on a crash, the log output will be present.
            // This is obviously not great in terms of performance, but for logging this is acceptable.
            logOutput->flush();
        }
    }

    static LogLevel GetLogLevelFilter();
    static std::filesystem::path GetLogFilePath();

    static void SetLogLevelFilter(LogLevel newFilter);
    // Change directory in which the log file we be created. Will not change the name of the output file.
    static void SetLogFilePath(const std::filesystem::path& newLogFilePath);
    static void SetLogDestination(LogDestination::Flags newDestinations);

private:
    void OpenLogFile();
    void CloseLogFile();

    // Closes the stream and renames the log file with the current timestamp.
    void CommitTimestampedLogFile();

    static constexpr const char* LogFileName = "vex";
    static constexpr const char* LogFileFormat = ".log";
    static constexpr const char* LogFileNameFormat = "vex.log";

    // Calls to log with a lower level than this will be ignored.
    LogLevel levelFilter = Info;
    LogDestination::Flags destinationFlags = LogDestination::Console | LogDestination::File;

    std::filesystem::path filePath = std::filesystem::current_path() / "logs" / LogFileNameFormat;
    std::optional<std::ofstream> logOutput;
};

inline Logger GLogger;

} // namespace vex

// Doing logging like this instead of with a function allows for DebugBreak to break in the actual code, avoiding us
// having to move up once in the call stack to get to the the actual code causing the error.

// Logs a potentially formatted string with one of the following log levels: Info, Warning, Error, Fatal.
// This follows std::format()'s formatting.
#define VEX_LOG(level, message, ...)                                                                                   \
    if ((level) >= vex::Logger::GetLogLevelFilter())                                                                   \
    {                                                                                                                  \
        vex::GLogger.Log((level), message, ##__VA_ARGS__);                                                             \
        if ((level) == vex::LogLevel::Fatal) /* Fatal error! Must exit. */                                             \
        {                                                                                                              \
            VEX_DEBUG_BREAK();                                                                                         \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    }
