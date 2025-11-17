#pragma once
#include <string_view>

void SetupPixEvents();
void StartPixEventsCapture(std::wstring_view captureName);
void EndPixEventsCapture();