#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>
#include <VexMacros.h>
// Keep this include, allows for implicit conversion of enums to enum string when using the Logger.
#include <Vex/Utility/MagicEnum.h>

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

constexpr std::string_view LogLevelToString(LogLevel logLevel)
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
enum class LogDestination : u8
{
    None    = 0,
    Console = 1 << 0, 
    File    = 1 << 1,
};
VEX_ENUM_FLAG_BITS(LogDestination);
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
        // We keep actual log logic in .cpp to reduce header bloat.
        LogToOutput(timeStampedFormatMessage);
    }

    static LogLevel GetLogLevelFilter();
    static std::filesystem::path GetLogFilePath();

    static void SetLogLevelFilter(LogLevel newFilter);
    // Change directory in which the log file we be created. Will not change the name of the output file.
    static void SetLogFilePath(const std::filesystem::path& newLogFilePath);
    static void SetLogDestination(Flags<LogDestination> newDestinations);

private:
    void OpenLogFile();
    void CloseLogFile();
    void LogToOutput(const std::string& logMessage);

    // Closes the stream and renames the log file with the current timestamp.
    void CommitTimestampedLogFile();

    static constexpr auto LogFileName = "vex";
    static constexpr auto LogFileFormat = ".log";
    static constexpr auto LogFileNameFormat = "vex.log";

    // Calls to log with a lower level than this will be ignored.
    LogLevel levelFilter = Info;
    Flags<LogDestination> destinationFlags = LogDestination::Console | LogDestination::File;

    std::filesystem::path filePath = std::filesystem::current_path() / "logs" / LogFileNameFormat;
    std::optional<std::ofstream> logOutput;
};

inline Logger GLogger;

} // namespace vex
