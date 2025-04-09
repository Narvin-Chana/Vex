#pragma once

#include <expected>
#include <source_location>

#include <Vex/Logger.h>

#include "VkHeaders.h"

namespace vex::vk
{

inline std::string FormatLocation(const std::source_location& loc)
{
    return std::format("{}:{}", loc.file_name(), loc.line());
}

template <class T>
std::expected<void, std::string> Validate(const ::vk::ResultValue<T>& val, std::source_location loc)
{
    if (val.result != ::vk::Result::eSuccess)
    {
        return std::unexpected(
            std::format("Result {} encoutered in {}", ::vk::to_string(val.result), FormatLocation(loc)));
    }

    return {};
}

template <class T>
void SanitizeOrCrash(const ::vk::ResultValue<T>& val, std::source_location loc)
{
    Validate(val, std::move(loc))
        .transform_error(
            [](const std::string& msg)
            {
                VEX_LOG(Fatal, "Validation failed: {}", msg);
                return msg;
            });
}

template <bool>
struct Sanitizer
{
    std::source_location loc = std::source_location::current();
};

#define CHECK                                                                                                          \
    Sanitizer<true>                                                                                                    \
    {                                                                                                                  \
    }

template <class T, bool b>
void operator>>(const ::vk::ResultValue<T>& val, Sanitizer<b>&& san)
{
    val << san;
}

template <class T>
void operator<<(Sanitizer<true>&& san, const ::vk::ResultValue<T>& val)
{
    SanitizeOrCrash(val, std::move(san.loc));
}

template <class T>
T operator<<=(Sanitizer<true>&& s, ::vk::ResultValue<T>&& val)
{
    SanitizeOrCrash(val, std::move(s.loc));
    return std::move(val.value);
}

template <std::convertible_to<bool> T, bool b>
T operator<<=(Sanitizer<b>&& s, T&& t)
{
    if (!t)
    {
        VEX_LOG(b ? Fatal : Error, "Condition failed at: {}", FormatLocation(s.loc));
    }

    return std::forward<T>(t);
}

#define CHECK_SOFT                                                                                                     \
    Sanitizer<false>                                                                                                   \
    {                                                                                                                  \
    }

template <class T>
std::expected<T, std::string> operator<<=(Sanitizer<false>&& s, ::vk::ResultValue<T>&& val)
{
    return Validate(val, std::move(s.loc))
        .and_then([&] { return std::expected<T, std::string>(std::move(val.value)); });
}

template <class T>
std::expected<void, std::string> operator<<(Sanitizer<false>&& san, const ::vk::ResultValue<T>& val)
{
    return Validate(val, std::move(san.loc));
}

} // namespace vex::vk