#pragma once

template <class... Ts>
struct Visitor : Ts...
{
    using Ts::operator()...;
};