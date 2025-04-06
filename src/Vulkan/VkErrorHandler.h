#pragma once

#include <expected>
#include <source_location>

#include <Vex/Logger.h>

#include "VkHeaders.h"

namespace vex::vk
{

template <class T>
std::expected<void, std::string> Validate(const ::vk::ResultValue<T>& val, std::source_location loc)
{
    if (val.result != ::vk::Result::eSuccess)
    {
        return std::unexpected(std::format("Result {} encoutered in {} at line {}",
                                           ::vk::to_string(val.result),
                                           loc.file_name(),
                                           loc.line()));
    }

    return {};
}

template <class T>
T&& Sanitize(::vk::ResultValue<T>&& val, std::source_location loc = std::source_location::current())
{
    if (std::string errorsString = Validate(val, std::move(loc)).error_or({}); !errorsString.empty())
    {
        VEX_LOG(Error, "{}", errorsString);
    }
    return std::move(val.value);
}
//
// struct Wrapper
// {
//     struct Inner
//     {
//         virtual ~Inner() = default;
//         virtual ::vk::Result GetResult() = 0;
//     };
//
//     template<class T>
//     struct InnerImpl : Inner
//     {
//         ::vk::ResultValue<T> lib;
//         virtual ::vk::Result GetResult()
//         {
//             return lib.result;
//         }
//     };
//
//     template<class T>
//     Wrapper(::vk::ResultValue<T> res, std::source_location loc = std::source_location::current())
//         : inner{std::make_unique<InnerImpl>(std::move(res))}
//     , loc{std::move(loc)}
//     {}
//
//     template<class T>
//     operator T()
//     {
//         return std::move(reinterpret_cast<InnerImpl<T>>(inner.get()).lib.value);
//     }
//
//     std::unique_ptr<Inner> inner;
//     std::source_location loc;
// };
//
// inline struct Checker{} chk;
//
// template<class T>
// void func(Checker, Wrapper wrapper)
// {
//     // do some stuff with wrapper
// }

//
// struct CheckerToken{};
//
// template<class T>
// void operator>>(const VkResultValue<T>& val, CheckerToken, std::source_location loc =
// std::source_location::current())
// {
//     Validate(val, loc);
// }
//
// template<class T>
// void operator<<(CheckerToken, const VkResultValue<T>& val, std::source_location loc =
// std::source_location::current())
// {
//     Validate(val, loc);
// }
//
// struct CheckerTokenSoft{};
//
// template<class T>
// std::expected<void, std::string> operator>>(const VkResultValue<T>& val, CheckerTokenSoft, std::source_location loc =
// std::source_location::current())
// {
//     return Validate(val, loc);;
// }
//
// template<class T>
// std::expected<void, std::string> operator<<(CheckerTokenSoft, const VkResultValue<T>& val, std::source_location loc =
// std::source_location::current())
// {
//     return Validate(val, loc);;
// }
//
// inline CheckerToken chk;
// inline CheckerTokenSoft chkSoft;
//
// template<std::convertible_to<bool> T>
// T&& operator<<=(CheckerToken, T&& t)
// {
//     assert(t);
//     return std::forward<T>(t);
// }
//
// template<class T>
// T&& operator<<=(CheckerToken, VkResultValue<T>&& val, std::source_location loc = std::source_location::current())
// {
//     if (std::string errorsString = Validate(val, loc).error_or({}); !errorsString.empty())
//     {
//         VEX_LOG(Error, "{}", errorsString);
//     }
//     return std::move(val.result.value);
// }

} // namespace vex::vk