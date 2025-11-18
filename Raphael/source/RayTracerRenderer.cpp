#include "RayTracerRenderer.h"

bool RayTracerRenderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
{
    if (!Renderer::Initialize(device, swapChain, hwnd))
        return false;

    // Reset command list for initialization
    FrameContext* frameContext = device.WaitForNextFrame();
    frameContext->CommandAllocator->Reset();
    device.GetCommandList()->Reset(frameContext->CommandAllocator, nullptr);

    BuildDescriptorHeaps(device);
    BuildConstantBuffers(device);
    BuildRootSignature(device);
    BuildShadersAndInputLayout();
	BuildFullscreenGeometry(device);
    BuildPSO(device);

    // Initialize camera position
    mPos = XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
    mFront = -1 * mPos;
    mUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // ADD: Initialize matrices
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    float aspect = static_cast<float>(clientRect.right) / clientRect.bottom;

    // Initialize world matrix (identity)
    XMStoreFloat4x4(&mWorld, XMMatrixIdentity());

    // Initialize projection matrix
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspect, 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, proj);

    // Execute the initialization commands
    device.GetCommandList()->Close();
    ID3D12CommandList* cmdLists[] = { device.GetCommandList() };
    device.GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
    // device.SignalAndIncrementFence(frameContext);

    // Wait until initialization is complete
    device.WaitForGpu();

    return true;
}

void RayTracerRenderer::Shutdown()
{
    m_fullscreenGeo.reset();
    m_sceneCB.reset();
    m_pso.Reset();
    m_rootSignature.Reset();
    m_cbvHeap.Reset();

    Renderer::Shutdown();
}

void RayTracerRenderer::BuildDescriptorHeaps(D3D12Device& device)
{
    // Implementation for creating descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1; // Only one constant buffer
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    if (FAILED(device.GetDevice()->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap))))
        throw std::runtime_error("Failed to create CBV descriptor heap");
}

void RayTracerRenderer::BuildConstantBuffers(D3D12Device& device)
{
    // Implementation for creating constant buffers
    m_sceneCB = std::make_unique<UploadBuffer<SceneConstants>>(device.GetDevice(), 1, true);

    UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(SceneConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_sceneCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(SceneConstants));

    device.GetDevice()->CreateConstantBufferView(
        &cbvDesc,
        m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void RayTracerRenderer::BuildRootSignature(D3D12Device& device)
{
    // Implementation for creating root signature
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf())))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    if (FAILED(device.GetDevice()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
    {
        throw std::runtime_error("Failed to create root signature");
    }
}

void RayTracerRenderer::BuildShadersAndInputLayout()
{
    // Implementation for compiling shaders and defining input layout
    m_vsByteCode = d3dUtil::CompileShader(L"Shaders\\simpleGpuRt.hlsl", nullptr, "VS_Main", "vs_5_0");
    m_psByteCode = d3dUtil::CompileShader(L"Shaders\\simpleGpuRt.hlsl", nullptr, "PS_Main", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void RayTracerRenderer::BuildFullscreenGeometry(D3D12Device& device)
{
    // Implementation for creating fullscreen geometry (a quad)
    std::vector<Vertex> vertices =
    {
        { XMFLOAT3(-1.0f,  1.0f, 0.0f) }, // Top-left
        { XMFLOAT3( 1.0f,  1.0f, 0.0f) }, // Top-right
        { XMFLOAT3(-1.0f, -1.0f, 0.0f) }, // Bottom-left
        { XMFLOAT3( 1.0f, -1.0f, 0.0f) }  // Bottom-right
    };

    std::vector<UINT> indices =
    {
        0, 1, 2,
        2, 1, 3
    };

    const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(UINT));

    m_fullscreenGeo = std::make_unique<MeshGeometry>();
    m_fullscreenGeo->Name = "fullscreen_quad";

    D3DCreateBlob(vbByteSize, &m_fullscreenGeo->VertexBufferCPU);
    CopyMemory(m_fullscreenGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &m_fullscreenGeo->IndexBufferCPU);
    CopyMemory(m_fullscreenGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    m_fullscreenGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.GetDevice(), device.GetCommandList(),
        vertices.data(), vbByteSize, m_fullscreenGeo->VertexBufferUploader);
    m_fullscreenGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.GetDevice(), device.GetCommandList(),
        indices.data(), ibByteSize, m_fullscreenGeo->IndexBufferUploader);

    m_fullscreenGeo->VertexByteStride = sizeof(Vertex);
    m_fullscreenGeo->VertexBufferByteSize = vbByteSize;
    m_fullscreenGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    m_fullscreenGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = static_cast<UINT>(indices.size());
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

	m_fullscreenGeo->DrawArgs["quad"] = submesh;
}

void RayTracerRenderer::BuildPSO(D3D12Device& device)
{
    // Implementation for creating pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
    psoDesc.RasterizerState = rsDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = device.GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = device.GetDepthStencilFormat();

    if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso))))
        throw std::runtime_error("Failed to create pipeline state object");
}

void RayTracerRenderer::Render(const ImVec4& clearColor)
{
    ImGui::Render();

    FrameContext* frameContext = m_device->WaitForNextFrame();
    UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    frameContext->CommandAllocator->Reset();

    ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList();
    cmdList->Reset(frameContext->CommandAllocator, nullptr);

    // TODO: Record commands and the rest
    // ADD: Set viewport and scissor rect (CRITICAL!)
    RECT clientRect;
    GetClientRect(GetActiveWindow(), &clientRect);
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(clientRect.right - clientRect.left);
    viewport.Height = static_cast<float>(clientRect.bottom - clientRect.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = clientRect.right;
    scissorRect.bottom = clientRect.bottom;

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // Indicate a state transition on the resource usage.
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetBackBuffer(backBufferIdx),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);

    // Clear the back buffer and depth buffer.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(backBufferIdx);
    // Clear and set render target
    const float clearColorArray[4] = {
        clearColor.x * clearColor.w,
        clearColor.y * clearColor.w,
        clearColor.z * clearColor.w,
        clearColor.w
    };
    cmdList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
    cmdList->ClearDepthStencilView(m_device->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_device->DepthStencilView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    cmdList->SetGraphicsRootSignature(GetRootSignature().Get());

    cmdList->SetPipelineState(m_pso.Get());

    D3D12_VERTEX_BUFFER_VIEW vbView = m_fullscreenGeo->VertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibView = m_fullscreenGeo->IndexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);
    cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    cmdList->DrawIndexedInstanced(
        m_fullscreenGeo->DrawArgs["quad"].IndexCount,
        1, 0, 0, 0);

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

void RayTracerRenderer::Update()
{
    XMMATRIX view = XMMatrixLookAtLH(mPos, mPos + mFront, mUp);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    // XMMATRIX worldViewProj = world * view * proj;
    XMMATRIX viewProj = view * proj;

    // Compute inverse view-projection matrix for ray generation
    XMMATRIX viewProjInverse = XMMatrixInverse(nullptr, viewProj);
    
    // Update the constant buffer with the latest worldViewProj matrix.
    SceneConstants sceneConstants;
    // Initialize scene parameters
    XMStoreFloat4(&sceneConstants.CameraPos, mPos);                 // Update camera position
    sceneConstants.Sphere = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);        // Sphere at origin, radius 1
    sceneConstants.Plane = XMFLOAT4(0.0f, 1.0f, 0.0f, 2.0f);         // Plane with normal (0,1,0), distance 2
    sceneConstants.LightPos = XMFLOAT4(3.0f, 3.0f, -3.0f, 1.0f);         // Light position
    sceneConstants.SphereColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);       // Red sphere
    sceneConstants.PlaneColor = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);        // Green plane

    XMStoreFloat4x4(&sceneConstants.InvViewProj, XMMatrixTranspose(viewProjInverse));
    m_sceneCB->CopyData(0, sceneConstants);
}

template<typename T>
static T Clamp(const T& x, const T& low, const T& high)
{
    return x < low ? low : (x > high ? high : x);
}

void RayTracerRenderer::ImGuiOnMouseDown(ImGuiMouseButton button, float x, float y)
{
    mLastMousePos.x = static_cast<LONG>(x);
    mLastMousePos.y = static_cast<LONG>(y);
}

void RayTracerRenderer::ImGuiOnMouseMove(ImGuiMouseButton button, float x, float y)
{
    if (button == ImGuiMouseButton_Left)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(4 * cameraSpeed * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(4 * cameraSpeed * static_cast<float>(y - mLastMousePos.y));

        mPitch += dy;
        mYaw += dx;
    }
    else if (button == ImGuiMouseButton_Right)
    {
        // TODO Implement up and down
    }

    // Calculate the new front vector
    mFront = XMVectorSet(
        cosf(mPitch) * sinf(mYaw),
        sinf(mPitch),
        cosf(mPitch) * cosf(mYaw),
        0.0f
    );

    mLastMousePos.x = static_cast<LONG>(x);
    mLastMousePos.y = static_cast<LONG>(y);
}
