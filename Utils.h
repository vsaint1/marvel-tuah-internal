#pragma once

// WORKAROUND FIX
#define NDEBUG
#ifndef NO_STATIC_ASSERT
#define STATIC_ASSERT(...)
#else
#define STATIC_ASSERT(...) static_assert(__VA_ARGS__)
#endif

#include "game/SDK/Engine_classes.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <shlobj.h>
#include <sstream>
#include <thread>
#include <unordered_set>

#define LOG 1

#if LOG
#include <stdio.h>
#define U_LOG(fmt, ...)                                                        \
  printf("%s:%d: " fmt " \r\n", __FILE__, __LINE__, __VA_ARGS__)
#else
#define U_LOG(fmt, ...)
#endif

#if LOG
#define JZ(x)                                                                  \
  if (!x) {                                                                    \
    U_LOG("JZ failed at %s:%d\n", __FILE__, __LINE__);                         \
    return;                                                                    \
  }
#else
#define JZ(x)                                                                  \
  if (!x) {                                                                    \
    return;                                                                    \
  }
#endif

inline SDK::UFont *pFont =
    SDK::UFont::FindObject<SDK::UFont>("FontFace RobotoRegular.RobotoRegular");

namespace Utils_Globals {
inline std::string windowTitle = "Unknown";

}

inline BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
  DWORD processId;
  GetWindowThreadProcessId(hWnd, &processId);

  if (processId == *(DWORD *)lParam) {
    char windowTitle[256];
    GetWindowTextA(hWnd, windowTitle, sizeof(windowTitle));

    if (strlen(windowTitle) > 0) {
      Utils_Globals::windowTitle = windowTitle;
      return FALSE;
    }
  }
  return TRUE;
};

namespace std {
template <> struct hash<SDK::FName> {
  size_t operator()(const SDK::FName &fName) const {
    return std::hash<std::string>{}(fName.ToString());
  }
};
} // namespace std

namespace Utils {

inline std::string WCharToString(const wchar_t *wstr) {
  if (!wstr)
    return "";

  size_t requiredSize = 0;
  wcstombs_s(&requiredSize, nullptr, 0, wstr, 0);

  char *buffer = new char[requiredSize];

  size_t convertedSize = 0;
  wcstombs_s(&convertedSize, buffer, requiredSize, wstr, _TRUNCATE);

  std::string result(buffer);

  delete[] buffer;

  return result;
}
inline std::wstring ToWString(const std::string &str) {

  std::wstring wStr(str.begin(), str.end());

  return wStr;
}

inline SDK::FName ToFName(const wchar_t *str) {
  return SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(str));
}

inline DWORD GetProcessId() {
  DWORD processId = GetCurrentProcessId();

  return processId;
}

inline std::string GetWndowTitle() {
  DWORD processId = GetCurrentProcessId();

  Utils_Globals::windowTitle.clear();

  EnumWindows(EnumWindowsProc, (LPARAM)&processId);

  return Utils_Globals::windowTitle;
};

struct FScreenSize {
  int width, height = 0;
};

inline FScreenSize GetScreenSize() {

  FScreenSize screen{.width = GetSystemMetrics(SM_CXSCREEN),
                     .height = GetSystemMetrics(SM_CYSCREEN)};

  return screen;
}

inline std::string GetDocumentsPath() {
  char path[MAX_PATH];
  if (SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT,
                       path) == S_OK) {
    return std::string(path);
  }
  return "";
}

inline void DumpProcessEvents(const std::string &fileName, SDK::UObject *pObj,
                              SDK::UFunction *pFunc) {
  std::ofstream logFile;
  std::string documentsPath = GetDocumentsPath();
  if (!documentsPath.empty()) {
    logFile.open(documentsPath + "\\" + fileName + ".txt", std::ios::app);

    if (logFile.is_open()) {
      // logFile << "Object: " << pObj->GetName()
      //<< ", Function: " << pFunc->GetName() << "\n";
      logFile.close();
    }
  }
}

inline void ListAvailableMaterials() {
  for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
    SDK::UObject *obj = SDK::UObject::GObjects->GetByIndex(i);
    if (obj == nullptr)
      continue;

    if (obj && obj->IsA(SDK::UMaterial::StaticClass())) {

      // U_LOG("Material: (%d,%s) \r\n", i, obj->GetFullName().c_str());
    }
  }
}

inline void ListAvailableFonts() {
  for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i) {
    SDK::UObject *obj = SDK::UObject::GObjects->GetByIndex(i);
    if (obj == nullptr)
      continue;

    if (obj && obj->IsA(SDK::UFont::StaticClass())) {
      // U_LOG("Font: (%d,%s) \r\n", i, obj->GetFullName().c_str());
    }
  }
}

} // namespace Utils