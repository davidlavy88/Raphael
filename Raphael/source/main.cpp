// Dear ImGui: standalone example application for Windows API + DirectX 12

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <tchar.h>
#include <cassert>
#include <memory>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// Config constants
static constexpr int NUM_FRAMES_IN_FLIGHT = 2;
static constexpr int NUM_BACK_BUFFERS = 2;
static constexpr int SRV_HEAP_SIZE = 64;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Frame context structure
struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator = nullptr;
    UINT64 FenceValue = 0;
    
    ~FrameContext()
    {
        if (CommandAllocator) {
            CommandAllocator->Release();
            CommandAllocator = nullptr;
        }
    }
};

// Descriptor heap allocator class
class DescriptorHeapAllocator
{
public:
    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(m_heap == nullptr && m_freeIndices.empty());
        
        m_heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        m_heapType = desc.Type;
        m_heapStartCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_heapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
        m_handleIncrement = device->GetDescriptorHandleIncrementSize(m_heapType);
        
        m_freeIndices.reserve(static_cast<int>(desc.NumDescriptors));
        for (int n = desc.NumDescriptors; n > 0; n--)
            m_freeIndices.push_back(n - 1);
    }

    void Shutdown()
    {
        m_heap = nullptr;
        m_freeIndices.clear();
    }

    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
    {
        IM_ASSERT(m_freeIndices.Size > 0);
        int idx = m_freeIndices.back();
        m_freeIndices.pop_back();
        outCpuHandle->ptr = m_heapStartCpu.ptr + (idx * m_handleIncrement);
        outGpuHandle->ptr = m_heapStartGpu.ptr + (idx * m_handleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
    {
        int cpuIdx = static_cast<int>((cpuHandle.ptr - m_heapStartCpu.ptr) / m_handleIncrement);
        int gpuIdx = static_cast<int>((gpuHandle.ptr - m_heapStartGpu.ptr) / m_handleIncrement);
        IM_ASSERT(cpuIdx == gpuIdx);
        m_freeIndices.push_back(cpuIdx);
    }

private:
    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGpu{};
    UINT m_handleIncrement = 0;
    ImVector<int> m_freeIndices;
};

// D3D12 Device wrapper class
class D3D12Device
{
public:
    bool Initialize()
    {
        // Enable debug layer
#ifdef DX12_ENABLE_DEBUG_LAYER
        ID3D12Debug* debug = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer();
        }
#endif

        // Create device
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_device))))
            return false;

        // Setup debug interface to break on errors
#ifdef DX12_ENABLE_DEBUG_LAYER
        if (debug != nullptr)
        {
            ID3D12InfoQueue* infoQueue = nullptr;
            m_device->QueryInterface(IID_PPV_ARGS(&infoQueue));
            if (infoQueue)
            {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
                infoQueue->Release();
            }
            debug->Release();
        }
#endif

        // Create command queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 1;
        if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))))
            return false;

        // Create descriptor heaps
        if (!CreateDescriptorHeaps())
            return false;

        // Create command allocators for each frame
        for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        {
            if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContexts[i].CommandAllocator))))
                return false;
        }

        // Create command list
        if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
            m_frameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_commandList))))
            return false;

        if (FAILED(m_commandList->Close()))
            return false;

        // Create fence
        if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
            return false;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
            return false;

        return true;
    }

    void Shutdown()
    {
        WaitForGpu();

        for (auto& frameContext : m_frameContexts)
            frameContext.~FrameContext();

        if (m_commandList) { m_commandList->Release(); m_commandList = nullptr; }
        if (m_commandQueue) { m_commandQueue->Release(); m_commandQueue = nullptr; }
        if (m_rtvHeap) { m_rtvHeap->Release(); m_rtvHeap = nullptr; }
        if (m_srvHeap) { m_srvHeap->Release(); m_srvHeap = nullptr; }
        if (m_fence) { m_fence->Release(); m_fence = nullptr; }
        if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }

#ifdef DX12_ENABLE_DEBUG_LAYER
        IDXGIDebug1* debug = nullptr;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
        {
            debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
            debug->Release();
        }
#endif
    }

    ID3D12Device* GetDevice() const { return m_device; }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue; }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList; }
    ID3D12DescriptorHeap* GetRTVHeap() const { return m_rtvHeap; }
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap; }
    DescriptorHeapAllocator& GetSRVAllocator() { return m_srvAllocator; }

    FrameContext* WaitForNextFrame()
    {
        FrameContext* frameContext = &m_frameContexts[m_frameIndex % NUM_FRAMES_IN_FLIGHT];
        if (m_fence->GetCompletedValue() < frameContext->FenceValue)
        {
            m_fence->SetEventOnCompletion(frameContext->FenceValue, m_fenceEvent);
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
        }
        return frameContext;
    }

    void SignalAndIncrementFence(FrameContext* frameContext)
    {
        m_commandQueue->Signal(m_fence, ++m_fenceLastSignaled);
        frameContext->FenceValue = m_fenceLastSignaled;
    }

    void WaitForGpu()
    {
        m_commandQueue->Signal(m_fence, ++m_fenceLastSignaled);
        m_fence->SetEventOnCompletion(m_fenceLastSignaled, m_fenceEvent);
        ::WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    void IncrementFrameIndex() { m_frameIndex++; }

private:
    bool CreateDescriptorHeaps()
    {
        // Create RTV heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = NUM_BACK_BUFFERS;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 1;
        if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
            return false;

        // Create SRV heap
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = SRV_HEAP_SIZE;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap))))
            return false;

        m_srvAllocator.Initialize(m_device, m_srvHeap);
        return true;
    }

private:
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    DescriptorHeapAllocator m_srvAllocator;
    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceLastSignaled = 0;
    UINT m_frameIndex = 0;
    FrameContext m_frameContexts[NUM_FRAMES_IN_FLIGHT];
};

// SwapChain wrapper class
class SwapChain
{
public:
    bool Initialize(HWND hwnd, D3D12Device& device)
    {
        // Setup swap chain description
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
        swapChainDesc.Width = 0;
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.Stereo = FALSE;

        // Create DXGI factory
        IDXGIFactory5* dxgiFactory = nullptr;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))))
            return false;

        // Check for tearing support
        BOOL allowTearing = FALSE;
        dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        m_tearingSupport = (allowTearing == TRUE);
        if (m_tearingSupport)
            swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        // Create swap chain
        IDXGISwapChain1* swapChain1 = nullptr;
        if (FAILED(dxgiFactory->CreateSwapChainForHwnd(device.GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1)))
        {
            dxgiFactory->Release();
            return false;
        }

        if (FAILED(swapChain1->QueryInterface(IID_PPV_ARGS(&m_swapChain))))
        {
            swapChain1->Release();
            dxgiFactory->Release();
            return false;
        }

        if (m_tearingSupport)
            dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        swapChain1->Release();
        dxgiFactory->Release();

        m_swapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        m_waitableObject = m_swapChain->GetFrameLatencyWaitableObject();

        // Create render target views
        CreateRenderTargetViews(device);
        return true;
    }

    void Shutdown()
    {
        if (m_swapChain) 
        { 
            m_swapChain->SetFullscreenState(false, nullptr);
            m_swapChain->Release();
            m_swapChain = nullptr;
        }
        
        if (m_waitableObject) 
        { 
            CloseHandle(m_waitableObject);
            m_waitableObject = nullptr;
        }

        CleanupRenderTargetViews();
    }

    void Resize(UINT width, UINT height, D3D12Device& device)
    {
        CleanupRenderTargetViews();
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        m_swapChain->GetDesc1(&desc);
        HRESULT result = m_swapChain->ResizeBuffers(0, width, height, desc.Format, desc.Flags);
        assert(SUCCEEDED(result) && "Failed to resize swapchain.");
        CreateRenderTargetViews(device);
    }

    HRESULT Present(bool vsync = true)
    {
        UINT syncInterval = vsync ? 1 : 0;
        UINT presentFlags = (!vsync && m_tearingSupport) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
        m_occluded = (hr == DXGI_STATUS_OCCLUDED);
        return hr;
    }

    UINT GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const { return m_rtvHandles[index]; }
    bool IsOccluded() const { return m_occluded; }
    HANDLE GetWaitableObject() const { return m_waitableObject; }

private:
    void CreateRenderTargetViews(D3D12Device& device)
    {
        SIZE_T rtvDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = device.GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
        
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            ID3D12Resource* backBuffer = nullptr;
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
            device.GetDevice()->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
            m_backBuffers[i] = backBuffer;
            m_rtvHandles[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    void CleanupRenderTargetViews()
    {
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            if (m_backBuffers[i]) 
            {
                m_backBuffers[i]->Release();
                m_backBuffers[i] = nullptr;
            }
        }
    }

private:
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12Resource* m_backBuffers[NUM_BACK_BUFFERS] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[NUM_BACK_BUFFERS] = {};
    bool m_tearingSupport = false;
    bool m_occluded = false;
    HANDLE m_waitableObject = nullptr;
};

// Renderer class that handles rendering operations
class Renderer
{
public:
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
    {
        m_device = &device;
        m_swapChain = &swapChain;

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);

        ImGui_ImplDX12_InitInfo initInfo = {};
        initInfo.Device = device.GetDevice();
        initInfo.CommandQueue = device.GetCommandQueue();
        initInfo.NumFramesInFlight = NUM_FRAMES_IN_FLIGHT;
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        initInfo.SrvDescriptorHeap = device.GetSRVHeap();
        initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) 
        { 
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Alloc(out_cpu_handle, out_gpu_handle); 
        };
        initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) 
        { 
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Free(cpu_handle, gpu_handle); 
        };
        
        // Store this pointer for callbacks
        ImGui::GetIO().UserData = this;
        
        ImGui_ImplDX12_Init(&initInfo);

        return true;
    }

    void Shutdown()
    {
        if (m_device)
            m_device->WaitForGpu();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void NewFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void Render(const ImVec4& clearColor)
    {
        ImGui::Render();

        FrameContext* frameContext = m_device->WaitForNextFrame();
        UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();
        
        frameContext->CommandAllocator->Reset();

        ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList();
        cmdList->Reset(frameContext->CommandAllocator, nullptr);

        // Transition to render target
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_swapChain->GetBackBuffer(backBufferIdx);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmdList->ResourceBarrier(1, &barrier);

        // Clear and set render target
        const float clearColorArray[4] = { 
            clearColor.x * clearColor.w, 
            clearColor.y * clearColor.w, 
            clearColor.z * clearColor.w, 
            clearColor.w 
        };
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(backBufferIdx);
        cmdList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        
        ID3D12DescriptorHeap* srvHeap = m_device->GetSRVHeap();
        cmdList->SetDescriptorHeaps(1, &srvHeap);

        // Render ImGui
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

        // Transition back to present
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmdList->ResourceBarrier(1, &barrier);
        cmdList->Close();

        // Execute commands
        ID3D12CommandList* cmdLists[] = { cmdList };
        m_device->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
        m_device->SignalAndIncrementFence(frameContext);
    }

private:
    D3D12Device* m_device = nullptr;
    SwapChain* m_swapChain = nullptr;
};

// Application class that manages the overall application
class Application
{
public:
    bool Initialize()
    {
        // Make process DPI aware and obtain main monitor scale
        ImGui_ImplWin32_EnableDpiAwareness();
        m_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

        // Create application window
        if (!CreateAppWindow())
            return false;

        // Initialize D3D12 components
        if (!m_device.Initialize())
        {
            DestroyAppWindow();
            return false;
        }

        if (!m_swapChain.Initialize(m_hwnd, m_device))
        {
            m_device.Shutdown();
            DestroyAppWindow();
            return false;
        }

        if (!m_renderer.Initialize(m_device, m_swapChain, m_hwnd))
        {
            m_swapChain.Shutdown();
            m_device.Shutdown();
            DestroyAppWindow();
            return false;
        }

        // Setup ImGui scaling
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(m_dpiScale);
        style.FontScaleDpi = m_dpiScale;

        // Show window
        ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(m_hwnd);

        return true;
    }

    void Shutdown()
    {
        m_renderer.Shutdown();
        m_swapChain.Shutdown();
        m_device.Shutdown();
        DestroyAppWindow();
    }

    void Run()
    {
        // Application state
        bool showDemoWindow = true;
        bool showAnotherWindow = false;
        ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        // Main loop
        bool done = false;
        while (!done)
        {
            // Process messages
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                    done = true;
            }
            if (done)
                break;

            // Handle window occlusion
            if ((m_swapChain.IsOccluded() && m_swapChain.Present(false) == DXGI_STATUS_OCCLUDED) || ::IsIconic(m_hwnd))
            {
                ::Sleep(10);
                continue;
            }

            // Wait for swap chain if needed
            if (HANDLE waitableObject = m_swapChain.GetWaitableObject())
                ::WaitForSingleObject(waitableObject, INFINITE);

            // Start ImGui frame
            m_renderer.NewFrame();

            // Demo windows
            if (showDemoWindow)
                ImGui::ShowDemoWindow(&showDemoWindow);

            // Main window
            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!");
                ImGui::Text("This is some useful text.");
                ImGui::Checkbox("Demo Window", &showDemoWindow);
                ImGui::Checkbox("Another Window", &showAnotherWindow);
                ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
                ImGui::ColorEdit3("clear color", (float*)&clearColor);

                if (ImGui::Button("Button"))
                    counter++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);

                ImGuiIO& io = ImGui::GetIO();
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::End();
            }

            // Another window
            if (showAnotherWindow)
            {
                ImGui::Begin("Another Window", &showAnotherWindow);
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    showAnotherWindow = false;
                ImGui::End();
            }

            // Render frame
            m_renderer.Render(clearColor);

            // Present
            m_swapChain.Present(true);
            m_device.IncrementFrameIndex();
        }
    }

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_SIZE:
            if (m_device.GetDevice() != nullptr && wParam != SIZE_MINIMIZED)
            {
                m_swapChain.Resize(LOWORD(lParam), HIWORD(lParam), m_device);
            }
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

private:
    bool CreateAppWindow()
    {
        WNDCLASSEXW wc = { 
            sizeof(wc), CS_CLASSDC, StaticWndProc, 0L, 0L, 
            GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, 
            L"ImGui Example", nullptr 
        };
        
        ::RegisterClassExW(&wc);
        
        m_hwnd = ::CreateWindowW(
            wc.lpszClassName, L"Dear ImGui DirectX12 Example", WS_OVERLAPPEDWINDOW,
            100, 100, static_cast<int>(1280 * m_dpiScale), static_cast<int>(800 * m_dpiScale),
            nullptr, nullptr, wc.hInstance, this
        );

        return m_hwnd != nullptr;
    }

    void DestroyAppWindow()
    {
        if (m_hwnd)
        {
            ::DestroyWindow(m_hwnd);
            ::UnregisterClassW(L"ImGui Example", GetModuleHandle(nullptr));
            m_hwnd = nullptr;
        }
    }

    static LRESULT WINAPI StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Application* app = nullptr;
        
        if (msg == WM_NCCREATE)
        {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = static_cast<Application*>(cs->lpCreateParams);
            ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        else
        {
            app = reinterpret_cast<Application*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (app)
            return app->HandleMessage(hWnd, msg, wParam, lParam);

        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

private:
    HWND m_hwnd = nullptr;
    float m_dpiScale = 1.0f;
    D3D12Device m_device;
    SwapChain m_swapChain;
    Renderer m_renderer;
};

// Entry point
int main(int, char**)
{
    Application app;
    
    if (!app.Initialize())
        return 1;

    app.Run();
    app.Shutdown();

    return 0;
}
