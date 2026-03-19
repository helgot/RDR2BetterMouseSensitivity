#include "renderer_hook_d3d12.hpp"

#include <MinHook.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <vector>
#include <windows.h>

#include "logger.hpp"
#include "user_interface.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

extern bool MENU_VISIBLE;

typedef HRESULT(WINAPI *D3D12CreateDevicePtr)(IUnknown *, D3D_FEATURE_LEVEL,
                                              REFIID, void **);
typedef HRESULT(WINAPI *PresentPtr)(IDXGISwapChain3 *, UINT, UINT);
typedef HRESULT(WINAPI *ResizeBuffersPtr)(IDXGISwapChain *, UINT, UINT, UINT,
                                          DXGI_FORMAT, UINT);
typedef void(WINAPI *ExecuteCommandListsPtr)(ID3D12CommandQueue *, UINT,
                                             ID3D12CommandList *const *);

// Original function pointers:
static D3D12CreateDevicePtr OriginalD3D12CreateDevice = nullptr;
static PresentPtr OriginalPresent = nullptr;
static ResizeBuffersPtr OriginalResizeBuffers = nullptr;
static ExecuteCommandListsPtr OriginalExecuteCommandLists = nullptr;

// DX12 objects captured from the game or created for ImGui
static ID3D12Device *Device = nullptr;
static ID3D12CommandQueue *CommandQueue = nullptr;
static IDXGISwapChain3 *SwapChain = nullptr;
static ID3D12DescriptorHeap *RtvDescriptorHeap =
    nullptr; // For Render Target Views
static ID3D12DescriptorHeap *SrvDescriptorHeap =
    nullptr; // For Shader Resource Views (ImGui font)
static std::vector<ID3D12Resource *> MainRenderTargetResource;
static std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> MainRenderTargetDescriptor;

// Per-frame resources for command recording
static std::vector<ID3D12CommandAllocator *> CommandAllocators;
static UINT NumBackBuffers = 0;

// Synchronization objects
static ID3D12Fence *Fence = nullptr;
static UINT64 FenceValue = 0;
static HANDLE FenceEvent = nullptr;
static bool ImguiInitialized = false;

// Win32 Objects:
static HWND WindowHandle = nullptr;
static WNDPROC OriginalWindowProc = nullptr;

// ImGui initialization and cleanup:
static void InitializeImGui(IDXGISwapChain3 *pSwapChain);
static void DeinitializeImGui();

// DX12 resource management
static void CreateRenderTarget();
static void CleanupRenderTarget();

// Hooked DX12/DXGI functions:
static HRESULT WINAPI Hooked_Present(IDXGISwapChain3 *pSwapChain,
                                     UINT SyncInterval, UINT Flags);
static HRESULT WINAPI Hooked_ResizeBuffers(IDXGISwapChain *pSwapChain,
                                           UINT BufferCount, UINT Width,
                                           UINT Height, DXGI_FORMAT NewFormat,
                                           UINT SwapChainFlags);
static void WINAPI
Hooked_ExecuteCommandLists(ID3D12CommandQueue *pQueue, UINT NumCommandLists,
                           ID3D12CommandList *const *ppCommandLists);
static HRESULT WINAPI Hooked_D3D12CreateDevice(
    IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
    void **ppDevice);

// Hooked WndProc function:
static LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lParam);

bool d3d12_hook_install()
{
    WNDCLASSEX WindowClass = {
        sizeof(WNDCLASSEX),    CS_CLASSDC, DefWindowProc, 0L,   0L,
        GetModuleHandle(NULL), NULL,       NULL,          NULL, NULL,
        "DX12 Dummy Window",   NULL};
    RegisterClassEx(&WindowClass);
    HWND DummyWindowHandle = CreateWindow(
        WindowClass.lpszClassName, "DX12 Dummy", WS_OVERLAPPEDWINDOW, 100, 100,
        300, 300, NULL, NULL, WindowClass.hInstance, NULL);

    ID3D12Device *Device = nullptr;
    ID3D12CommandQueue *CommandQueue = nullptr;
    IDXGISwapChain *TmpSwapChain = nullptr;
    IDXGISwapChain3 *SwapChain = nullptr;

    // Create device
    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&Device))))
    {
        DestroyWindow(DummyWindowHandle);
        UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
        LOG_DEBUG("Failed to create dummy D3D12 device.");
        return false;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(Device->CreateCommandQueue(&QueueDesc,
                                          IID_PPV_ARGS(&CommandQueue))))
    {
        Device->Release();
        DestroyWindow(DummyWindowHandle);
        UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
        LOG_DEBUG("Failed to create dummy command queue.");
        return false;
    }

    // Create swap chain
    IDXGIFactory4 *Factory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&Factory));
    if (Factory)
    {
        DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
        SwapChainDesc.BufferCount = 2;
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.OutputWindow = DummyWindowHandle;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.Windowed = TRUE;

        if (SUCCEEDED(Factory->CreateSwapChain(CommandQueue, &SwapChainDesc,
                                               &TmpSwapChain)))
        {
            TmpSwapChain->QueryInterface(IID_PPV_ARGS(&SwapChain));
            TmpSwapChain->Release();
        }
        Factory->Release();
    }

    if (!SwapChain)
    {
        CommandQueue->Release();
        Device->Release();
        DestroyWindow(DummyWindowHandle);
        UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
        LOG_DEBUG("Failed to create dummy swap chain.");
        return false;
    }

    // Get V-table addresses
    void **pVTable = *(void ***)SwapChain;
    LPVOID TargetPresent = (LPVOID)pVTable[8];
    LPVOID TargetResizeBuffers = (LPVOID)pVTable[13];

    void **pVTable_Queue = *(void ***)CommandQueue;
    LPVOID TargetExecuteCommandLists = pVTable_Queue[10];

    LOG_DEBUG("%s: OriginalPresent: 0x%llx", __func__, OriginalPresent);
    LOG_DEBUG("%s: OriginalResizeBuffers: 0x%llx", __func__,
              OriginalResizeBuffers);

    // Cleanup dummy objects
    SwapChain->Release();
    CommandQueue->Release();
    Device->Release();
    DestroyWindow(DummyWindowHandle);
    UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);

    // Create and enable hooks
    if (MH_CreateHook(TargetPresent, (LPVOID)Hooked_Present,
                      (LPVOID *)&OriginalPresent) != MH_OK)
    {
        LOG_DEBUG("Failed to create hook for Present!");
        return false;
    }
    if (MH_EnableHook(TargetPresent) != MH_OK)
    {
        LOG_DEBUG("Failed to enable hook for Present!");
        return false;
    }
    LOG_DEBUG("Hook for Present enabled.");

    if (MH_CreateHook(TargetResizeBuffers, (LPVOID)Hooked_ResizeBuffers,
                      (LPVOID *)&OriginalResizeBuffers) != MH_OK)
    {
        LOG_DEBUG("Failed to create hook for ResizeBuffers!");
        return false;
    }
    if (MH_EnableHook(TargetResizeBuffers) != MH_OK)
    {
        LOG_DEBUG("Failed to enable hook for ResizeBuffers!");
        return false;
    }
    LOG_DEBUG("Hook for ResizeBuffers enabled.");

    // ExecuteCommandLists Hook
    if (MH_CreateHook(
            TargetExecuteCommandLists, (LPVOID)Hooked_ExecuteCommandLists,
            reinterpret_cast<void **>(&OriginalExecuteCommandLists)) != MH_OK ||
        MH_EnableHook(TargetExecuteCommandLists) != MH_OK)
    {
        LOG_DEBUG("Failed to hook ExecuteCommandLists!");
        return false;
    }
    LOG_DEBUG("Hook for ExecuteCommandLists enabled.");

    LOG_DEBUG("%s: OriginalPresent: 0x%llx", __func__, OriginalPresent);
    LOG_DEBUG("%s: OriginalResizeBuffers: 0x%llx", __func__,
              OriginalResizeBuffers);
    LOG_DEBUG("%s: OriginalExecuteCommandLists: 0x%llx", __func__,
              OriginalExecuteCommandLists);

    LOG_DEBUG("DirectX 12 API hooks installed successfully.");
    return true;
}

void d3d12_hook_shutdown()
{
    // Wait for the GPU to be done with all resources.
    if (CommandQueue && Fence && FenceEvent)
    {
        const UINT64 CurrentFence = FenceValue;
        CommandQueue->Signal(Fence, CurrentFence);
        FenceValue++;
        if (Fence->GetCompletedValue() < CurrentFence)
        {
            Fence->SetEventOnCompletion(CurrentFence, FenceEvent);
            WaitForSingleObject(FenceEvent, INFINITE);
        }
    }

    DeinitializeImGui();

    MH_DisableHook((LPVOID)OriginalPresent);
    MH_DisableHook((LPVOID)OriginalResizeBuffers);
    MH_DisableHook((LPVOID)OriginalExecuteCommandLists);
    LOG_DEBUG("DirectX 12 Hooks Disabled.");
}

static void InitializeImGui(IDXGISwapChain3 *pSwapChain)
{
    if (ImguiInitialized)
        return;

    if (!Device || !CommandQueue)
    {
        LOG_DEBUG(
            "Device or Command Queue not captured. Cannot initialize ImGui.");
        return;
    }

    LOG_DEBUG("Initializing ImGui for DX12...");

    DXGI_SWAP_CHAIN_DESC Desc;
    pSwapChain->GetDesc(&Desc);
    NumBackBuffers = Desc.BufferCount;
    WindowHandle = Desc.OutputWindow;

    // Create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc = {};
    RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RtvHeapDesc.NumDescriptors = NumBackBuffers;
    RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(Device->CreateDescriptorHeap(&RtvHeapDesc,
                                            IID_PPV_ARGS(&RtvDescriptorHeap))))
    {
        LOG_DEBUG("FATAL: Failed to create RTV descriptor heap.");
        return;
    }

    // Create SRV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
    SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    SrvHeapDesc.NumDescriptors = 1;
    SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(Device->CreateDescriptorHeap(&SrvHeapDesc,
                                            IID_PPV_ARGS(&SrvDescriptorHeap))))
    {
        LOG_DEBUG("FATAL: Failed to create SRV descriptor heap.");
        // Free allocated resourced up to this point in the initalization.
        if (RtvDescriptorHeap)
            RtvDescriptorHeap->Release();
        RtvDescriptorHeap = nullptr;
        return;
    }

    CommandAllocators.resize(NumBackBuffers);
    for (int BufferIdx = 0; BufferIdx < NumBackBuffers; BufferIdx++)
    {
        if (FAILED(Device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&CommandAllocators[BufferIdx]))))
        {
            LOG_DEBUG("FATAL: Failed to create command allocator for frame %u.",
                      BufferIdx);
            // TODO(harrison): Clean-up.
            return;
        }
    }
    LOG_DEBUG("Created %u command allocators.", NumBackBuffers);

    // Create render target views for swap chain buffers
    CreateRenderTarget();

    // Create synchronization objects
    if (FAILED(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&Fence))))
    {
        LOG_DEBUG("FATAL: Failed to create fence.");
        // TODO(harrison): Clean-up.
        return;
    }
    FenceValue = 1;
    FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    IO.MouseDrawCursor = MENU_VISIBLE;

    ImGui::StyleColorsDark();

    // Hook WndProc
    OriginalWindowProc = (WNDPROC)SetWindowLongPtrA(WindowHandle, GWLP_WNDPROC,
                                                    (LONG_PTR)Hooked_WndProc);
    ImGui_ImplWin32_Init(WindowHandle);

    // Setup the modern InitInfo struct
    ImGui_ImplDX12_InitInfo InitInfo = {};
    InitInfo.Device = Device;
    InitInfo.NumFramesInFlight = NumBackBuffers;
    InitInfo.RTVFormat = Desc.BufferDesc.Format;
    InitInfo.CommandQueue = CommandQueue;
    InitInfo.SrvDescriptorHeap = SrvDescriptorHeap;
    InitInfo.LegacySingleSrvCpuDescriptor =
        SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    InitInfo.LegacySingleSrvGpuDescriptor =
        SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Call the modern Init function
    if (ImGui_ImplDX12_Init(&InitInfo))
    {
        ImguiInitialized = true;
        LOG_DEBUG("ImGui for DX12 initialized successfully.");
        // TODO(harrison): Clean-up.
    }
    else
    {
        LOG_DEBUG("FATAL: ImGui_ImplDX12_Init failed.");
        ImGui_ImplWin32_Shutdown();
        SetWindowLongPtrA(WindowHandle, GWLP_WNDPROC,
                          (LONG_PTR)OriginalWindowProc);
        // TODO(harrison): Clean-up.
    }
}

static void DeinitializeImGui()
{
    if (!ImguiInitialized)
        return;

    LOG_DEBUG("Shutting down ImGui DX12 backend.");

    // Restore original WndProc
    if (OriginalWindowProc)
    {
        SetWindowLongPtrA(WindowHandle, GWLP_WNDPROC,
                          (LONG_PTR)OriginalWindowProc);
        OriginalWindowProc = nullptr;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupRenderTarget();

    if (SrvDescriptorHeap)
    {
        SrvDescriptorHeap->Release();
        SrvDescriptorHeap = nullptr;
    }
    if (RtvDescriptorHeap)
    {
        RtvDescriptorHeap->Release();
        RtvDescriptorHeap = nullptr;
    }

    for (auto &Allocator : CommandAllocators)
    {
        if (Allocator)
        {
            Allocator->Release();
        }
    }
    CommandAllocators.clear();

    if (CommandQueue)
    {
        CommandQueue->Release();
        CommandQueue = nullptr;
    }

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }

    if (Fence)
    {
        Fence->Release();
        Fence = nullptr;
    }
    if (FenceEvent)
    {
        CloseHandle(FenceEvent);
        FenceEvent = nullptr;
    }
    ImguiInitialized = false;
    LOG_DEBUG("ImGui DX12 backend shut down.");
}

static void CreateRenderTarget()
{
    MainRenderTargetDescriptor.resize(NumBackBuffers);
    MainRenderTargetResource.resize(NumBackBuffers);

    UINT RtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle =
        RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT BufferIdx = 0; BufferIdx < NumBackBuffers; BufferIdx++)
    {
        MainRenderTargetDescriptor[BufferIdx] = RtvHandle;
        SwapChain->GetBuffer(
            BufferIdx, IID_PPV_ARGS(&MainRenderTargetResource[BufferIdx]));
        Device->CreateRenderTargetView(MainRenderTargetResource[BufferIdx],
                                       nullptr, RtvHandle);
        RtvHandle.ptr += RtvDescriptorSize;
    }
}

static void CleanupRenderTarget()
{
    for (auto &Resource : MainRenderTargetResource)
    {
        if (Resource)
            Resource->Release();
    }
    MainRenderTargetResource.clear();
    MainRenderTargetDescriptor.clear();
}

static LRESULT CALLBACK Hooked_WndProc(HWND WindowHandle, UINT Message,
                                       WPARAM WParam, LPARAM LParam)
{
    if (ImguiInitialized)
    {
        if (ImGui_ImplWin32_WndProcHandler(WindowHandle, Message, WParam,
                                           LParam))
        {
            ImGuiIO &IO = ImGui::GetIO();
            if (IO.WantCaptureMouse || IO.WantCaptureKeyboard)
            {
                return TRUE;
            }
        }
    }

    handle_input();

    return CallWindowProc(OriginalWindowProc, WindowHandle, Message, WParam,
                          LParam);
}

static HRESULT WINAPI Hooked_D3D12CreateDevice(
    IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
    void **ppDevice)
{
    LOG_DEBUG("D3D12CreateDevice called, enabling debug layer...");

    ID3D12Debug *pDebugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
    {
        pDebugController->EnableDebugLayer();
        pDebugController->Release();
        LOG_DEBUG("D3D12 Debug Layer ENABLED.");
    }
    else
    {
        LOG_DEBUG("Failed to get D3D12 Debug Interface. Is Graphics Tools "
                  "installed?");
    }

    return OriginalD3D12CreateDevice(pAdapter, MinimumFeatureLevel, riid,
                                     ppDevice);
}

HRESULT WINAPI Hooked_Present(IDXGISwapChain3 *pSwapChain, UINT SyncInterval,
                              UINT Flags)
{
    // Initialization logic: only proceed when all components are captured.
    if (!ImguiInitialized)
    {
        // Capture swapchain and device from this function
        if (SwapChain != pSwapChain)
        {
            SwapChain = pSwapChain;
            pSwapChain->GetDevice(IID_PPV_ARGS(&Device));
            LOG_DEBUG("Captured SwapChain and D3D12 Device.");
        }

        // If we have everything we need, initialize ImGui
        if (SwapChain && Device && CommandQueue)
        {
            InitializeImGui(pSwapChain);
        }
    }

    if (ImguiInitialized && MENU_VISIBLE)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGuiIO &IO = ImGui::GetIO();
        // Add an assertion to catch the problem immediately
        assert(IO.DisplaySize.x > 0.0f && IO.DisplaySize.y > 0.0f);
        ImGui::NewFrame();
        render_ui();
        ImGui::Render();

        UINT BufferIdx = pSwapChain->GetCurrentBackBufferIndex();
        ID3D12CommandAllocator *CommandAllocator = CommandAllocators[BufferIdx];
        CommandAllocator->Reset();

        ID3D12GraphicsCommandList *CommandList = nullptr;
        Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  CommandAllocator, NULL,
                                  IID_PPV_ARGS(&CommandList));

        // Transition back buffer to render target state
        D3D12_RESOURCE_BARRIER Barrier = {};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = MainRenderTargetResource[BufferIdx];
        Barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        CommandList->ResourceBarrier(1, &Barrier);

        // Set render target
        CommandList->OMSetRenderTargets(
            1, &MainRenderTargetDescriptor[BufferIdx], FALSE, nullptr);

        // Set descriptor heaps
        CommandList->SetDescriptorHeaps(1, &SrvDescriptorHeap);

        // Render ImGui draw data
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList);

        // Transition back buffer to present state
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        CommandList->ResourceBarrier(1, &Barrier);

        CommandList->Close();
        CommandQueue->ExecuteCommandLists(
            1, (ID3D12CommandList *const *)&CommandList);
        CommandList->Release();
    }

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

static HRESULT WINAPI Hooked_ResizeBuffers(IDXGISwapChain *pSwapChain,
                                           UINT BufferCount, UINT Width,
                                           UINT Height, DXGI_FORMAT NewFormat,
                                           UINT SwapChainFlags)
{
    if (ImguiInitialized)
    {
        LOG_DEBUG("ResizeBuffers called. Cleaning up ImGui render targets.");
        CleanupRenderTarget();
    }

    HRESULT Result = OriginalResizeBuffers(pSwapChain, BufferCount, Width,
                                           Height, NewFormat, SwapChainFlags);

    if (SUCCEEDED(Result) && ImguiInitialized)
    {
        LOG_DEBUG(
            "Recreating ImGui render targets for new swap chain buffers.");
        NumBackBuffers = BufferCount;
        CreateRenderTarget();
    }

    return Result;
}
void WINAPI Hooked_ExecuteCommandLists(ID3D12CommandQueue *pQueue,
                                       UINT NumCommandLists,
                                       ID3D12CommandList *const *ppCommandLists)
{
    if (!CommandQueue)
    {
        LOG_DEBUG("Captured game's D3D12 Command Queue: %p", pQueue);
        CommandQueue = pQueue;
    }
    OriginalExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
}