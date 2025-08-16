#include "Logger.h"

#include <ctime>

namespace vex
{

Logger::Logger()
{
    // Ensures the log file's path exists.
    SetLogFilePath(filePath);
}

Logger::~Logger()
{
    CommitTimestampedLogFile();
}

void Logger::SetLogLevelFilter(LogLevel newFilter)
{
    GLogger.levelFilter = newFilter;
}

void Logger::SetLogFilePath(const std::filesystem::path& newLogFilePath)
{
    bool validPath = true;
    std::filesystem::path strippedNewLogFilePath = std::filesystem::path(newLogFilePath).remove_filename();
    if (!std::filesystem::exists(strippedNewLogFilePath))
    {
        validPath |= std::filesystem::create_directories(strippedNewLogFilePath);
    }

#if defined(_WIN32)
    static constexpr size_t MaxPathLength = 256;
    // Windows only allows for paths shorter than 256.
    // If this is the case, we give up on changing the log file name.
    if (strippedNewLogFilePath.string().size() >= MaxPathLength)
    {
        VEX_LOG(Error, "Unable to set log path because the total path length would be greater than 256.");
        return;
    }
#endif

    if (validPath)
    {
        GLogger.filePath = strippedNewLogFilePath / LogFileNameFormat;
    }
}

void Logger::SetLogDestination(LogDestination::Flags newDestinations)
{
    GLogger.destinationFlags = newDestinations;
}

LogLevel Logger::GetLogLevelFilter()
{
    return GLogger.levelFilter;
}

std::filesystem::path Logger::GetLogFilePath()
{
    return GLogger.filePath;
}

void Logger::OpenLogFile()
{
    CloseLogFile();

    // Can't open empty filepath.
    if (filePath.empty())
    {
        return;
    }
    logOutput = std::ofstream(filePath, std::ios_base::app);
}

void Logger::CloseLogFile()
{
    if (!logOutput.has_value())
    {
        return;
    }

    logOutput.reset();
}

void Logger::CommitTimestampedLogFile()
{
    // Nothing to commit.
    if (!logOutput.has_value())
    {
        return;
    }

    CloseLogFile();

    std::string timeStamp = GetTimestampString();
    std::replace(timeStamp.begin(), timeStamp.end(), ' ', '_');
    std::replace(timeStamp.begin(), timeStamp.end(), ':', '-');

    std::string fileName = std::string(LogFileName).append(std::format("_{}", timeStamp)).append(LogFileFormat);
    std::filesystem::path timeStampedFilePath = std::filesystem::path(filePath).remove_filename() / fileName;

#if defined(_WIN32)
    // Windows only allows for paths shorter than 256.
    // If this is the case, we give up on renaming the log file.
    static constexpr size_t MaxPathLength = 256;
    if (timeStampedFilePath.string().size() >= MaxPathLength)
    {
        VEX_LOG(Error, "Unable to commit log because the total path length of the new file would be greater than 256.");
        return;
    }
#endif
    std::filesystem::rename(filePath, timeStampedFilePath);
}

std::string GetTimestampString()
{
    // Clang and GCC's linux STLs do not yet implement a formatter for time_point...
#if defined(_WIN32)
    auto const time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::format("{:%Y-%m-%d %H:%M:%OS}", time);
#elif defined(__linux__)
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return buf;
#endif
}

} // namespace vex