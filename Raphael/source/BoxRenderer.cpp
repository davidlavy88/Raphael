#include "BoxRenderer.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

bool BoxRenderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
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
    BuildBoxGeometry(device);
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

void BoxRenderer::Shutdown()
{
    mBoxGeo.reset();
    m_objectCB.reset();
    m_pso.Reset();
    m_rootSignature.Reset();
    m_cbvHeap.Reset();

    Renderer::Shutdown();
}

void BoxRenderer::BuildDescriptorHeaps(D3D12Device& device)
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

void BoxRenderer::BuildConstantBuffers(D3D12Device& device)
{
    // Implementation for creating constant buffers
    m_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device.GetDevice(), 1, true);

    UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_objectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(ObjectConstants));

    device.GetDevice()->CreateConstantBufferView(
        &cbvDesc,
        m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxRenderer::BuildRootSignature(D3D12Device& device)
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

void BoxRenderer::BuildShadersAndInputLayout()
{
    // Implementation for compiling shaders and defining input layout
    m_vsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    m_psByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void BoxRenderer::BuildBoxGeometry(D3D12Device& device)
{
    // Implementation for creating box geometry
    std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	// Needs finishing setting up vertex and index buffers
    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU);
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU);
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.GetDevice(),
        device.GetCommandList(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.GetDevice(),
        device.GetCommandList(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["box"] = submesh;
}

void BoxRenderer::BuildPSO(D3D12Device& device)
{
    // Implementation for creating pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
    /*rsDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rsDesc.CullMode = D3D12_CULL_MODE_FRONT;*/
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

void BoxRenderer::Render(const ImVec4& clearColor)
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

	D3D12_VERTEX_BUFFER_VIEW vbView = mBoxGeo->VertexBufferView();
	D3D12_INDEX_BUFFER_VIEW ibView = mBoxGeo->IndexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);
    cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    cmdList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["box"].IndexCount,
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

void BoxRenderer::Update(float deltaTime)
{
    XMMATRIX view = XMMatrixLookAtLH(mPos, mPos + mFront, mUp);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    m_objectCB->CopyData(0, objConstants);
}
