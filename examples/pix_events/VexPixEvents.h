#pragma once
#include <string_view>

namespace PIX
{
void Setup();
void StartCapture(std::wstring_view captureName);
void EndCapture();
} // namespace PIX
