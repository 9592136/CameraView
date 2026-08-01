#pragma once

#include <windows.h>

#include <vector>

struct WindowControlDefinition {
    int control_id = 0;
    const wchar_t* class_name = nullptr;
    const wchar_t* text = nullptr;
    DWORD style = 0;
};

class WindowControlDefinitions {
public:
    static const std::vector<WindowControlDefinition>& All();
    static const WindowControlDefinition* Find(int control_id);
};
