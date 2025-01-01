#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#include "MinHook\Include\MinHook.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"

//=========================================================================================================================//
#include "Settings.h"
#include <fstream>

#define _X(...) (__VA_ARGS__)

using namespace std;

char dlldir[320];
char* GetDirectoryFile(char* filename)
{
	static char path[320];
	strcpy_s(path, dlldir);
	strcat_s(path, filename);
	return path;
}

void Log(const char* fmt, ...)
{
	if (!fmt)	return;

	char		text[4096];
	va_list		ap;
	va_start(ap, fmt);
	vsprintf_s(text, fmt, ap);
	va_end(ap);

	ofstream logfile(GetDirectoryFile((PCHAR)"log.txt"), ios::app);
	if (logfile.is_open() && text)	logfile << text << endl;
	logfile.close();
}

//=========================================================================================================================//

WNDCLASSEX WindowClass;
HWND WindowHwnd;

bool InitWindow() {

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = DefWindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandle(NULL);
	WindowClass.hIcon = NULL;
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = NULL;
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = "MJ";
	WindowClass.hIconSm = NULL;
	RegisterClassEx(&WindowClass);
	WindowHwnd = CreateWindow(WindowClass.lpszClassName, "DirectX Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, WindowClass.hInstance, NULL);
	if (WindowHwnd == NULL) {
		return false;
	}
	return true;
}

bool DeleteWindow() {
	DestroyWindow(WindowHwnd);
	UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
	if (WindowHwnd != NULL) {
		return false;
	}
	return true;
}

#if defined _M_X64
typedef uint64_t uintx_t;
#elif defined _M_IX86
typedef uint32_t uintx_t;
#endif

static uintx_t* MethodsTable = NULL;

//=========================================================================================================================//


namespace DirectX12 {
	bool Init() {

		if (InitWindow() == false) {
			return false;
		}

		HMODULE D3D12Module = GetModuleHandle(_X("d3d12.dll"));
		HMODULE DXGIModule = GetModuleHandle(_X("dxgi.dll"));
		if (D3D12Module == NULL || DXGIModule == NULL) {
			DeleteWindow();
			return false;
		}

		void* CreateDXGIFactory = GetProcAddress(DXGIModule, _X("CreateDXGIFactory"));
		if (CreateDXGIFactory == NULL) {
			DeleteWindow();
			return false;
		}

		IDXGIFactory* Factory;
		if (((long(__stdcall*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&Factory) < 0) {
			DeleteWindow();
			return false;
		}

		IDXGIAdapter* Adapter;
		if (Factory->EnumAdapters(0, &Adapter) == DXGI_ERROR_NOT_FOUND) {
			DeleteWindow();
			return false;
		}

		void* D3D12CreateDevice = GetProcAddress(D3D12Module, _X("D3D12CreateDevice"));
		if (D3D12CreateDevice == NULL) {
			DeleteWindow();
			return false;
		}

		ID3D12Device* Device;
		if (((long(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**))(D3D12CreateDevice))(Adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&Device) < 0) {
			DeleteWindow();
			return false;
		}

		D3D12_COMMAND_QUEUE_DESC QueueDesc;
		QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		QueueDesc.Priority = 0;
		QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		QueueDesc.NodeMask = 0;

		ID3D12CommandQueue* CommandQueue;
		if (Device->CreateCommandQueue(&QueueDesc, __uuidof(ID3D12CommandQueue), (void**)&CommandQueue) < 0) {
			DeleteWindow();
			return false;
		}

		ID3D12CommandAllocator* CommandAllocator;
		if (Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&CommandAllocator) < 0) {
			DeleteWindow();
			return false;
		}

		ID3D12GraphicsCommandList* CommandList;
		if (Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&CommandList) < 0) {
			DeleteWindow();
			return false;
		}

		DXGI_RATIONAL RefreshRate;
		RefreshRate.Numerator = 60;
		RefreshRate.Denominator = 1;

		DXGI_MODE_DESC BufferDesc;
		BufferDesc.Width = 100;
		BufferDesc.Height = 100;
		BufferDesc.RefreshRate = RefreshRate;
		BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC SampleDesc;
		SampleDesc.Count = 1;
		SampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
		SwapChainDesc.BufferDesc = BufferDesc;
		SwapChainDesc.SampleDesc = SampleDesc;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.BufferCount = 2;
		SwapChainDesc.OutputWindow = WindowHwnd;
		SwapChainDesc.Windowed = 1;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		IDXGISwapChain* SwapChain;
		if (Factory->CreateSwapChain(CommandQueue, &SwapChainDesc, &SwapChain) < 0) {
			DeleteWindow();
			return false;
		}

		MethodsTable = (uintx_t*)::calloc(150, sizeof(uintx_t));
		memcpy(MethodsTable, *(uintx_t**)Device, 44 * sizeof(uintx_t));
		memcpy(MethodsTable + 44, *(uintx_t**)CommandQueue, 19 * sizeof(uintx_t));
		memcpy(MethodsTable + 44 + 19, *(uintx_t**)CommandAllocator, 9 * sizeof(uintx_t));
		memcpy(MethodsTable + 44 + 19 + 9, *(uintx_t**)CommandList, 60 * sizeof(uintx_t));
		memcpy(MethodsTable + 44 + 19 + 9 + 60, *(uintx_t**)SwapChain, 18 * sizeof(uintx_t));

		MH_Initialize();
		Device->Release();
		Device = NULL;
		CommandQueue->Release();
		CommandQueue = NULL;
		CommandAllocator->Release();
		CommandAllocator = NULL;
		CommandList->Release();
		CommandList = NULL;
		SwapChain->Release();
		SwapChain = NULL;
		DeleteWindow();
		return true;
	}
}

//=========================================================================================================================//


bool CreateHook(uint16_t Index, void** Original, void* Function) {
	assert(_index >= 0 && _original != NULL && _function != NULL);
	void* target = (void*)MethodsTable[Index];
	if (MH_CreateHook(target, Function, Original) != MH_OK || MH_EnableHook(target) != MH_OK) {
		return false;
	}
	return true;
}

void DisableHook(uint16_t Index) {
	assert(Index >= 0);
	MH_DisableHook((void*)MethodsTable[Index]);
}

void DisableAll() {
	MH_DisableHook(MH_ALL_HOOKS);
	free(MethodsTable);
	MethodsTable = NULL;
}



inline bool waitingForKey =
    false;

inline std::string GetKeyName(int key) {
  static std::unordered_map<int, std::string> mouseKeyNames = {
      {VK_LBUTTON, "Left Mouse Button"},   {VK_RBUTTON, "Right Mouse Button"},
      {VK_MBUTTON, "Middle Mouse Button"}, {VK_XBUTTON1, "Mouse Button 4"},
      {VK_XBUTTON2, "Mouse Button 5"},
  };

  if (mouseKeyNames.count(key)) {
    return mouseKeyNames[key];
  }

  char name[128];
  if (GetKeyNameTextA(MapVirtualKeyA(key, MAPVK_VK_TO_VSC) << 16, name,
                      sizeof(name))) {
    return name;
  }

  return "Unknown";
}

inline void HotKeyBinding() {
  ImGui::Text(_X("Current Hotkey: %s"),
              (Settings::Aimbot::hotkey != 0x0)
                  ? GetKeyName(Settings::Aimbot::hotkey).c_str()
                  : "-");

  if (!waitingForKey) {
    if (ImGui::Button(_X("Set Hotkey"))) {
      waitingForKey = true;
    }
  } else {
    ImGui::Text(_X("Press any key"));

    for (int key = 8; key <= 255; ++key) {
      if (GetAsyncKeyState(key) & 0x8000) {
        Settings::Aimbot::hotkey = key; 
        waitingForKey = false;
        return;
      }
    }

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
      Settings::Aimbot::hotkey = VK_LBUTTON;
      waitingForKey = false;
      return;
    }
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
      Settings::Aimbot::hotkey = VK_RBUTTON;
      waitingForKey = false;
      return;
    }
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
      Settings::Aimbot::hotkey = VK_MBUTTON;
      waitingForKey = false;
      return;
    }
    if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) {
      Settings::Aimbot::hotkey = VK_XBUTTON1;
      waitingForKey = false;
      return;
    }
    if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) {
      Settings::Aimbot::hotkey = VK_XBUTTON2;
      waitingForKey = false;
      return;
    }
  }
}


inline void DrawBox(int X, int Y, int W, int H, const ImU32 &color, int thickness) {
  float lineW = (W / 1);
  float lineH = (H / 1);
  ImDrawList *Drawlist = ImGui::GetForegroundDrawList();
  // black outlines
  Drawlist->AddLine(ImVec2(X, Y), ImVec2(X, Y + lineH),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X, Y), ImVec2(X + lineW, Y),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X + W - lineW, Y), ImVec2(X + W, Y),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + lineH),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X, Y + H - lineH), ImVec2(X, Y + H),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X, Y + H), ImVec2(X + lineW, Y + H),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X + W - lineW, Y + H), ImVec2(X + W, Y + H),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  Drawlist->AddLine(ImVec2(X + W, Y + H - lineH), ImVec2(X + W, Y + H),
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)),
                    3);
  // corners
  Drawlist->AddLine(ImVec2(X, Y), ImVec2(X, Y + lineH),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X, Y), ImVec2(X + lineW, Y),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X + W - lineW, Y), ImVec2(X + W, Y),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + lineH),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X, Y + H - lineH), ImVec2(X, Y + H),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X, Y + H), ImVec2(X + lineW, Y + H),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X + W - lineW, Y + H), ImVec2(X + W, Y + H),
                    ImGui::GetColorU32(color), thickness);
  Drawlist->AddLine(ImVec2(X + W, Y + H - lineH), ImVec2(X + W, Y + H),
                    ImGui::GetColorU32(color), thickness);
}
