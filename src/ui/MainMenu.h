#pragma once

#include <windows.h>

#include "i18n/Localization.h"

HMENU CreateMainMenu(UILanguage lang);
void SyncMainMenu(HWND hwnd);
