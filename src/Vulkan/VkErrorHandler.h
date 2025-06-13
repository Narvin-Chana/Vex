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

inline std::expected<void, std::string> Validate(const ::vk::Result& result, std::source_location loc)
{
    if (result != ::vk::Result::eSuccess)
    {
        return std::unexpected(std::format("Result {} encoutered in {}", ::vk::to_string(result), FormatLocation(loc)));
    }

    return {};
}

inline void SanitizeOrCrash(const ::vk::Result& result, std::source_location loc)
{
    if (auto exp = Validate(result, std::move(loc)); !exp.has_value())
    {
        VEX_LOG(Fatal, "Validation failed: {}", exp.error());
    }
}

template <bool ShouldCrash>
struct Sanitizer
{
    std::source_location loc = std::source_location::current();
};

#define VEX_VK_CHECK                                                                                                   \
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
    SanitizeOrCrash(val.result, std::move(san.loc));
}

inline void operator<<(Sanitizer<true>&& san, ::vk::Result result)
{
    SanitizeOrCrash(result, std::move(san.loc));
}

inline std::expected<void, std::string> operator<<(Sanitizer<false>&& san, ::vk::Result result)
{
    return Validate(result, std::move(san.loc));
}

template <class T>
T operator<<=(Sanitizer<true>&& s, ::vk::ResultValue<T>&& val)
{
    SanitizeOrCrash(val.result, std::move(s.loc));
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
    return Validate(val.result, std::move(s.loc))
        .and_then([&] { return std::expected<T, std::string>(std::move(val.value)); });
}

template <class T>
std::expected<void, std::string> operator<<(Sanitizer<false>&& san, const ::vk::ResultValue<T>& val)
{
    return Validate(val.result, std::move(san.loc));
}

} // namespace vex::vk