//*********************************************************
//
// Copyright 2020 Intel Corporation 
//
// Permission is hereby granted, free of charge, to any 
// person obtaining a copy of this software and associated 
// documentation files(the "Software"), to deal in the Software 
// without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to 
// whom the Software is furnished to do so, subject to the 
// following conditions :
// The above copyright notice and this permission notice shall 
// be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE.
//
//*********************************************************
#pragma once

#include "AdapterShared.h"
#include <DirectXMath.h>
#include "SimpleCamera.h"
#include "Compute.h" // for shared handles structure

class ExtensionHelper;

class Render : public AdapterShared
{
public:
    Render(HWND in_hwnd, UINT in_numParticles,
        IDXGIAdapter1* in_pAdapter,
        bool in_useIntelCommandQueueExtension,
        bool in_fullScreen, RECT in_windowDim);
    virtual ~Render();

    Render(const Render&) = delete;
    Render(Render&&) = delete;
    Render& operator=(const Render&) = delete;
    Render& operator=(Render&&) = delete;

    // Draw() tells Particles to draw its UI
    // input is compute fence value. output is render fence value.
    // normally, in_numParticlesCopied should equal in_numActiveParticles
    // in_numParticlesCopied was added to experiment with stressing the PCI bus
    HANDLE Draw(int in_numActiveParticles, class Particles* in_pParticles, UINT64& inout_fenceValue,
        int in_numParticlesCopied);

    void SetParticleSize(float in_particleSize) { m_particleSize = in_particleSize; }
    void SetParticleIntensity(float in_particleIntensity) { m_particleIntensity = in_particleIntensity; }

    //-----------------------------------------------------
    // used to create descriptor heap for UI
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    // used to initialize UI object
    static UINT GetNumFrames() { return NUM_FRAMES; }
    //-----------------------------------------------------

    //-----------------------------------------------------
    // Intel Command Queue Extension interfaces:
    bool GetSupportsIntelCommandQueueExtension() const;
    //-----------------------------------------------------

    //-----------------------------------------------------
    // for multi-adapter sharing
    HANDLE GetSharedFenceHandle() const { return m_sharedFenceHandle; }
    void SetShared(const Compute::SharedHandles& in_sharedHandles);
    //-----------------------------------------------------

    //-----------------------------------------------------
    // for async compute mode
    ComPtr<ID3D12Fence> GetFence() const { return m_renderFence; }
    ComPtr<ID3D12Resource>* const GetBuffers() { return m_buffers; }
    UINT GetBufferIndex() const { return m_currentBufferIndex; }
    void SetAsyncMode(bool in_enable) { m_asyncMode = in_enable; }
    //-----------------------------------------------------

    struct Particle
    {
        DirectX::XMFLOAT4 position;
    };

    // stalls until adapter is idle
    virtual void WaitForGpu() override;

private:
    bool m_asyncMode;

    static constexpr std::uint32_t NUM_FRAMES = 2;
    const UINT m_numParticles;

    ExtensionHelper* m_pExtensionHelper;
    HWND m_hwnd;
    ComPtr<IDXGIAdapter1> m_adapter;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<IDXGISwapChain3> m_swapChain;
    UINT m_frameIndex; // depends on NUM_FRAMES, not NUM_BUFFERS
    HANDLE m_swapChainEvent;
    UINT64 m_frameFenceValues[NUM_FRAMES];
    UINT64 m_renderFenceValue;
    ComPtr<ID3D12Fence> m_renderFence;
    HANDLE m_renderFenceEvent;

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[NUM_FRAMES];
    ComPtr<ID3D12CommandAllocator> m_copyAllocators[NUM_FRAMES];
    UINT m_rtvDescriptorSize;
    UINT m_srvUavDescriptorSize;

    ComPtr<ID3D12Resource> m_renderTargets[NUM_FRAMES];
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    enum GraphicsRootParameters : UINT32
    {
        GraphicsRootCBV = 0,
        GraphicsRootSRVTable,
        GraphicsRootParametersCount
    };

    // "Vertex" definition for particles.
    // Triangle vertices are generated by the geometry shader.
    // Color data will be assigned to those vertices via this struct.
    struct ParticleVertex
    {
        DirectX::XMFLOAT4 color;
    };

    // Position and velocity data for the particles in the system.
    // Two buffers full of Particle data are utilized in this sample.
    // The compute thread alternates writing to each of them.
    // The render thread renders using the buffer that is not currently
    // in use by the compute shader.

    struct ConstantBufferGS
    {
        DirectX::XMFLOAT4X4 worldViewProjection;
        DirectX::XMFLOAT4X4 inverseView;
        float particleSize;
        float particleIntensity;
        // Constant buffers are 256-byte aligned in GPU memory. Padding is added
        // for convenience when computing the struct's size.
        float padding[32-1-1];
    };

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    static constexpr UINT m_NUM_BUFFERS = 2;
    ComPtr<ID3D12Resource> m_buffers[m_NUM_BUFFERS];
    UINT64 m_bufferSize;

    // ping-pong buffer index
    UINT m_currentBufferIndex;

    ComPtr<ID3D12Resource> m_constantBufferGS;
    UINT8* m_pConstantBufferGSData; // re-used across device changes. destroy!
    SimpleCamera m_camera;
    float m_aspectRatio;

    void UpdateCamera();

    void CreateVertexBuffer();
    void CreateParticleBuffers();

    void CreateCommandQueue(); // calls CreateSwapChain() because swap chain depends on command queue
    void CreateSwapChain();
    void LoadAssets();

    // returns a handle (or 0), so the calling function can WaitOn/Multiple/
    HANDLE MoveToNextFrame();

    // copy command queue
    ComPtr<ID3D12CommandQueue> m_copyQueue;
    ComPtr<ID3D12GraphicsCommandList> m_copyList;
    ComPtr<ID3D12Fence> m_copyFence;
    UINT64 m_copyFenceValue;

    void CopySimulationResults(UINT64 in_fenceValue, int in_numActiveParticles);
    ComPtr<ID3D12Resource> m_sharedBuffers[m_NUM_BUFFERS];
    UINT m_sharedBufferIndex;

    bool m_fullScreen;
    bool m_windowedSupportsTearing;
    RECT m_windowDim;

    HANDLE m_sharedFenceHandle;
    float m_particleSize;
    float m_particleIntensity;

    ComPtr<ID3D12Fence> m_sharedComputeFence;
};
