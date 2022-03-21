//***************************************************************************************
// TreeBillboardsApp.cpp 
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class TreeBillboardsApp : public D3DApp
{
public:
    TreeBillboardsApp(HINSTANCE hInstance);
    TreeBillboardsApp(const TreeBillboardsApp& rhs) = delete;
    TreeBillboardsApp& operator=(const TreeBillboardsApp& rhs) = delete;
    ~TreeBillboardsApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt); 

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayouts();
    void BuildLandGeometry();
    void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildTreeSpritesGeometry();
	void BuildXgeometry();
	void BuildWallsGeometry();
	void BuildTowersGeometry();
	void BuildDiamondGeometry();
	void BuildTopTowersGeometry();
	void BuildGateGeometry();
	void BuildMerlonlGeometry();


    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
	void BuildRenderTowers();
	void BuildRenderGate();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        TreeBillboardsApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

TreeBillboardsApp::TreeBillboardsApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

TreeBillboardsApp::~TreeBillboardsApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool TreeBillboardsApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(200, 200, 2.0f, 0.03f, 4.0f, 0.2f);
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayouts();
    BuildLandGeometry();
    BuildWavesGeometry();
	BuildBoxGeometry();
	BuildTreeSpritesGeometry();
	BuildXgeometry();
	BuildWallsGeometry();
	BuildTowersGeometry();
	BuildDiamondGeometry();
	BuildTopTowersGeometry();
	BuildGateGeometry();
	BuildMerlonlGeometry();
	BuildMaterials();
    BuildRenderItems();
	BuildRenderTowers();
	BuildRenderGate();
	BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void TreeBillboardsApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void TreeBillboardsApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void TreeBillboardsApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::CornflowerBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void TreeBillboardsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void TreeBillboardsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void TreeBillboardsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void TreeBillboardsApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void TreeBillboardsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void TreeBillboardsApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if(tu >= 1.0f)
		tu -= 1.0f;

	if(tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void TreeBillboardsApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };
	//direction light
	mMainPassCB.Lights[0].Direction = { 0.0f, -0.27735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.3f, 0.3f, 0.5f };

	//point lights
																									   //
	mMainPassCB.Lights[1].Position = { -16.5f, 15.5f, +16.5f };										   //
	mMainPassCB.Lights[1].Strength = { 255.0f/4.0f, 192.0f / 4.0f, 203.0f / 4.0f };					   //
																									   //
	mMainPassCB.Lights[2].Position = { 16.5f, 15.5f, +16.5f };										   //
	mMainPassCB.Lights[2].Strength = { 255.0f / 4.0f, 192.0f / 4.0f, 203.0f / 4.0f };				   //
																									   // lights for the towers
	mMainPassCB.Lights[3].Position = { -16.5f, 15.5f, -16.5f };										   //
	mMainPassCB.Lights[3].Strength = { 255.0f / 4.0f, 192.0f / 4.0f, 203.0f / 4.0f };				   //
																									   //
	mMainPassCB.Lights[4].Position = { 16.5f, 15.5f, -16.5f };										   //
	mMainPassCB.Lights[4].Strength = { 255.0f / 4.0f, 192.0f / 4.0f, 203.0f / 4.0f };				   //
																
																									   
	mMainPassCB.Lights[5].Position = { 0.0f, 26.0f, 0.0f };										   //Centre light
	mMainPassCB.Lights[5].Strength = { 200.0f / 4.0f, 0.0f / 4.0f, 0.0f / 4.0f };				   //


	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void TreeBillboardsApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for(int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);
		
		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void TreeBillboardsApp::LoadTextures()
{
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../../Textures/ice.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/tree01S.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	auto WallTex = std::make_unique<Texture>();
	WallTex->Name = "WallTex";
	WallTex->Filename = L"../../Textures/wall.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), WallTex->Filename.c_str(),
		WallTex->Resource, WallTex->UploadHeap));

	auto WallTex2 = std::make_unique<Texture>();
	WallTex2->Name = "WallTex2";
	WallTex2->Filename = L"../../Textures/wall2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), WallTex2->Filename.c_str(),
		WallTex2->Resource, WallTex2->UploadHeap));

	auto WallTex3 = std::make_unique<Texture>();
	WallTex3->Name = "WallTex3";
	WallTex3->Filename = L"../../Textures/wall3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), WallTex3->Filename.c_str(),
		WallTex3->Resource, WallTex3->UploadHeap));

	auto sample1 = std::make_unique<Texture>();
	sample1->Name = "sample1";
	sample1->Filename = L"../../Textures/sample2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sample1->Filename.c_str(),
		sample1->Resource, sample1->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);

	mTextures[WallTex->Name] = std::move(WallTex);
	mTextures[WallTex2->Name] = std::move(WallTex2);
	mTextures[WallTex3->Name] = std::move(WallTex3);
	mTextures[sample1->Name] = std::move(sample1);
}

void TreeBillboardsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TreeBillboardsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 8;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
	auto WallTex = mTextures["WallTex"]->Resource;

	auto WallTex2 = mTextures["WallTex2"]->Resource;
	auto WallTex3 = mTextures["WallTex3"]->Resource;
	auto sample1 = mTextures["sample1"]->Resource; 
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
	
	
	

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	//// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = WallTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(WallTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
hDescriptor.Offset(1, mCbvSrvDescriptorSize);
srvDesc.Format = WallTex2->GetDesc().Format;
md3dDevice->CreateShaderResourceView(WallTex2.Get(), &srvDesc, hDescriptor);
// next descriptor
hDescriptor.Offset(1, mCbvSrvDescriptorSize);
srvDesc.Format = WallTex3->GetDesc().Format;
md3dDevice->CreateShaderResourceView(WallTex3.Get(), &srvDesc, hDescriptor);
// next descriptor
hDescriptor.Offset(1, mCbvSrvDescriptorSize);
srvDesc.Format = sample1->GetDesc().Format;
md3dDevice->CreateShaderResourceView(sample1.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);




	
}

void TreeBillboardsApp::BuildShadersAndInputLayouts()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mStdInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void TreeBillboardsApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(0.0f, 0.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for(size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for(int i = 0; i < m - 1; ++i)
    {
        for(int j = 0; j < n - 1; ++j)
        {
            indices[k] = i*n + j;
            indices[k + 1] = i*n + j + 1;
            indices[k + 2] = (i + 1)*n + j;

            indices[k + 3] = (i + 1)*n + j;
            indices[k + 4] = i*n + j + 1;
            indices[k + 5] = (i + 1)*n + j + 1;

            k += 6; // next quad
        }
    }

	UINT vbByteSize = mWaves->VertexCount()*sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size()*sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(85.0f, 0.2f, 85.0f, 0);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildTreeSpritesGeometry()
{
	//step5
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 6;
	std::array<TreeSpriteVertex, 16> vertices;
	float x, z;
	for(UINT i = 0; i < treeCount; ++i)
	{
		switch (i)
		{
		case 0:
			x = -40.0f;
			z = -40.0f;
			break;
		case 1:
			x = 40.0f;
			z = -40.0f;
			break;
		case 2:
			x = 40.0f;
			z = 40.0f;
			break;
		case 3:
			x = -40.0f;
			z = 40.0f;
			break;
		case 4:
			x = -40;
			z = 0.0f;
			break;
		case 5:
			x = 40;
			z = 0.0f;
			break;

		default:
			break;
		}
		float y = 2.5f;

		// Move tree slightly above land height.
		y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	}

	std::array<std::uint16_t, 6> indices =
	{
		0, 1, 2, 3, 4, 5
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}
void TreeBillboardsApp::BuildXgeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateCylinder(20.0f, 20.0f, 20.0f, 10.0f, 10.0f);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "xGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["x"] = submesh;

	mGeometries["xGeo"] = std::move(geo);
}
void TreeBillboardsApp::BuildWallsGeometry()
{
	//STEP 1 Define geometry
	GeometryGenerator geoGen;//Define size
	GeometryGenerator::MeshData m_Walls = geoGen.CreateBox(1.0f, 4.0f, 25.0f, 0);

	std::vector<Vertex> vertices(m_Walls.Vertices.size());
	for (size_t i = 0; i < m_Walls.Vertices.size(); ++i)
	{
		auto& p = m_Walls.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = m_Walls.Vertices[i].Normal;
		vertices[i].TexC = m_Walls.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = m_Walls.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "m_Walls_Geo"; // Name of unique geometry


	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["m_Walls"] = submesh;
	mGeometries["m_Walls_Geo"] = std::move(geo);


}
void TreeBillboardsApp::BuildTowersGeometry()
{

	//STEP 1 Define geometry
	GeometryGenerator geoGen;//Define size
	GeometryGenerator::MeshData m_Tower = geoGen.CreateCylinder(2.5f, 2.5f, 19.5f, 14, 33);

	std::vector<Vertex> vertices(m_Tower.Vertices.size());

	for (size_t i = 0; i < m_Tower.Vertices.size(); ++i)
	{
		auto& p = m_Tower.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = m_Tower.Vertices[i].Normal;
		vertices[i].TexC = m_Tower.Vertices[i].TexC;
	}


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	std::vector<std::uint16_t> indices = m_Tower.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "TowerGeo"; // Name of unique geometry


	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);



	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);



	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;


	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Tower"] = submesh;
	mGeometries["TowerGeo"] = std::move(geo);
}
void TreeBillboardsApp::BuildDiamondGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData m_Diamond = geoGen.CreateDiamond(2, 2, 2,4);

}
void TreeBillboardsApp::BuildTopTowersGeometry()
{
	GeometryGenerator geoGen;//Define size
	GeometryGenerator::MeshData m_TowerTopCones = geoGen.CreateCylinder(1.5f, 0.0f, 3.5f, 14, 33);

	std::vector<Vertex> vertices(m_TowerTopCones.Vertices.size());
	for (size_t i = 0; i < m_TowerTopCones.Vertices.size(); ++i)
	{
		auto& p = m_TowerTopCones.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = m_TowerTopCones.Vertices[i].Normal;
		vertices[i].TexC = m_TowerTopCones.Vertices[i].TexC;
	}
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	std::vector<std::uint16_t> indices = m_TowerTopCones.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "TowerTopGeo"; // Name of unique geometry


	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["TowerTop"] = submesh;
	mGeometries["TowerTopGeo"] = std::move(geo);




}
void TreeBillboardsApp::BuildGateGeometry()
{

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData m_Gate = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);

	std::vector<Vertex> vertices(m_Gate.Vertices.size());
	for (size_t i = 0; i < m_Gate.Vertices.size(); ++i)
	{
		auto& p = m_Gate.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = m_Gate.Vertices[i].Normal;
		vertices[i].TexC = m_Gate.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = m_Gate.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "GateGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Gate"] = submesh;

	mGeometries["GateGeo"] = std::move(geo);
}
void TreeBillboardsApp::BuildMerlonlGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData TriangularMerlon = geoGen.CreateCylinder(1.0f, 0.0f, 1.0f, 4, 4);

	std::vector<Vertex> vertices(TriangularMerlon.Vertices.size());
	for (size_t i = 0; i < TriangularMerlon.Vertices.size(); ++i)
	{
		auto& p = TriangularMerlon.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = TriangularMerlon.Vertices[i].Normal;
		vertices[i].TexC = TriangularMerlon.Vertices[i].TexC;
	}
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	std::vector<std::uint16_t> indices = TriangularMerlon.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "MerlonGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Merlon"] = submesh;
	mGeometries["MerlonGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;

	//there is abug with F2 key that is supposed to turn on the multisampling!
//Set4xMsaaState(true);
	//m4xMsaaState = true;

	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for tree sprites
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};
	//step1
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

void TreeBillboardsApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void TreeBillboardsApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;



	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;

	auto wall = std::make_unique<Material>();
	wall->Name = "wall";
	wall->MatCBIndex = 3;
	wall->DiffuseSrvHeapIndex = 3;
	wall->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wall->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wall->Roughness = 0.125f;

	auto wall2 = std::make_unique<Material>();
	wall2->Name = "wall2";
	wall2->MatCBIndex = 4;
	wall2->DiffuseSrvHeapIndex = 4;
	wall2->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wall2->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wall2->Roughness = 0.125f;

	auto wall3 = std::make_unique<Material>();
	wall3->Name = "wall3";
	wall3->MatCBIndex = 5;
	wall3->DiffuseSrvHeapIndex = 5;
	wall3->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wall3->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wall3->Roughness = 0.125f;

	auto sample1 = std::make_unique<Material>();
	sample1->Name = "sample1";
	sample1->MatCBIndex = 6;
	sample1->DiffuseSrvHeapIndex = 6;
	sample1->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 2.0f);
	sample1->FresnelR0 = XMFLOAT3(0.902f, 0.902f, 0.902f);
	sample1->Roughness = 0.902f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 7;
	treeSprites->DiffuseSrvHeapIndex = 7;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	



	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);
	mMaterials["wall"] = std::move(wall);
	mMaterials["wall2"] = std::move(wall2);
	mMaterials["wall3"] = std::move(wall3);
	mMaterials["sample1"] = std::move(sample1);
}

void TreeBillboardsApp::BuildRenderItems()
{

	UINT objCBIndex = 4;
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(0.0f, 1.0f, -0.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["grass"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());

	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 3;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	//step2
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());


	auto m_Wall_R1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&m_Wall_R1->TexTransform, XMMatrixScaling(5.0f, 5.0f, 2.0f));
	XMStoreFloat4x4(&m_Wall_R1->World, XMMatrixScaling(2.0f, 4.0f, 2.0f) * XMMatrixTranslation(25.0f, 7.5f, 0.0f));

	m_Wall_R1->ObjCBIndex = objCBIndex++;
	m_Wall_R1->Mat = mMaterials["wall"].get();
	m_Wall_R1->Geo = mGeometries["m_Walls_Geo"].get();
	m_Wall_R1->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_Wall_R1->IndexCount = m_Wall_R1->Geo->DrawArgs["m_Walls"].IndexCount;
	m_Wall_R1->StartIndexLocation = m_Wall_R1->Geo->DrawArgs["m_Walls"].StartIndexLocation;
	m_Wall_R1->BaseVertexLocation = m_Wall_R1->Geo->DrawArgs["m_Walls"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(m_Wall_R1.get());



	auto m_Wall_2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&m_Wall_2->TexTransform, XMMatrixScaling(5.0f, 5.0f, 2.0f));
	XMStoreFloat4x4(&m_Wall_2->World, XMMatrixScaling(2.0f, 4.0f, 2.0f) * XMMatrixTranslation(-25.0f, 7.5f, 0.0f));
	m_Wall_2->ObjCBIndex = objCBIndex++;
	m_Wall_2->Mat = mMaterials["wall"].get();
	m_Wall_2->Geo = mGeometries["m_Walls_Geo"].get();
	m_Wall_2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_Wall_2->IndexCount = m_Wall_2->Geo->DrawArgs["m_Walls"].IndexCount;
	m_Wall_2->StartIndexLocation = m_Wall_2->Geo->DrawArgs["m_Walls"].StartIndexLocation;
	m_Wall_2->BaseVertexLocation = m_Wall_2->Geo->DrawArgs["m_Walls"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(m_Wall_2.get());

	auto m_Wall_3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&m_Wall_3->TexTransform, XMMatrixScaling(5.0f, 5.0f, 2.0f));
	XMStoreFloat4x4(&m_Wall_3->World, XMMatrixScaling(50.0f, 4.0f, 0.1f) * XMMatrixTranslation(0.0f, 7.5f, -25.0f));
	m_Wall_3->ObjCBIndex = objCBIndex++;
	m_Wall_3->Mat = mMaterials["wall"].get();
	m_Wall_3->Geo = mGeometries["m_Walls_Geo"].get();
	m_Wall_3->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_Wall_3->IndexCount = m_Wall_3->Geo->DrawArgs["m_Walls"].IndexCount;
	m_Wall_3->StartIndexLocation = m_Wall_3->Geo->DrawArgs["m_Walls"].StartIndexLocation;
	m_Wall_3->BaseVertexLocation = m_Wall_3->Geo->DrawArgs["m_Walls"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(m_Wall_3.get());

	auto m_Wall_4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&m_Wall_4->TexTransform, XMMatrixScaling(5.0f, 5.0f, 2.0f));
	XMStoreFloat4x4(&m_Wall_4->World, XMMatrixScaling(50.0f, 4.0f, 0.1f) * XMMatrixTranslation(0.0f, 7.5f, 25.0f));
	m_Wall_4->ObjCBIndex = objCBIndex++;
	m_Wall_4->Mat = mMaterials["wall"].get();
	m_Wall_4->Geo = mGeometries["m_Walls_Geo"].get();
	m_Wall_4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_Wall_4->IndexCount = m_Wall_3->Geo->DrawArgs["m_Walls"].IndexCount;
	m_Wall_4->StartIndexLocation = m_Wall_4->Geo->DrawArgs["m_Walls"].StartIndexLocation;
	m_Wall_4->BaseVertexLocation = m_Wall_4->Geo->DrawArgs["m_Walls"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(m_Wall_4.get());

    mAllRitems.push_back(std::move(wavesRitem));
    mAllRitems.push_back(std::move(gridRitem));
	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));
	/*mAllRitems.push_back(std::move(boxRitem2));*/
	mAllRitems.push_back(std::move(m_Wall_R1));
	mAllRitems.push_back(std::move(m_Wall_2));
	mAllRitems.push_back(std::move(m_Wall_3));
	mAllRitems.push_back(std::move(m_Wall_4));
}

void TreeBillboardsApp::BuildRenderTowers()
{

	UINT objCBIndex = 8;

	//LEFT TOWER - UP /////////////////////////////////////
	auto leftTowerUP = std::make_unique<RenderItem>();
	auto RooftleftTowerUP = std::make_unique<RenderItem>();
	XMMATRIX leftTowerWorld = XMMatrixTranslation(-24.5f, 9.50f, +24.5f);
	XMMATRIX RooftleftTowerUPWorld = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-24.5f, 22.5f, +24.5f);

	XMStoreFloat4x4(&leftTowerUP->World, leftTowerWorld);
	XMStoreFloat4x4(&RooftleftTowerUP->World, RooftleftTowerUPWorld);

	leftTowerUP->ObjCBIndex = objCBIndex++;
	RooftleftTowerUP->ObjCBIndex = objCBIndex++;

	leftTowerUP->Mat = mMaterials["wall"].get();
	RooftleftTowerUP->Mat = mMaterials["wall3"].get();


	leftTowerUP->Geo = mGeometries["TowerGeo"].get();
	RooftleftTowerUP->Geo = mGeometries["TowerTopGeo"].get();
	leftTowerUP->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftleftTowerUP->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	leftTowerUP->IndexCount = leftTowerUP->Geo->DrawArgs["Tower"].IndexCount;
	RooftleftTowerUP->IndexCount = RooftleftTowerUP->Geo->DrawArgs["TowerTop"].IndexCount;


	leftTowerUP->StartIndexLocation = leftTowerUP->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftleftTowerUP->StartIndexLocation = RooftleftTowerUP->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	leftTowerUP->BaseVertexLocation = leftTowerUP->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftleftTowerUP->BaseVertexLocation = RooftleftTowerUP->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(leftTowerUP.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftleftTowerUP.get());

	mAllRitems.push_back(std::move(leftTowerUP));
	mAllRitems.push_back(std::move(RooftleftTowerUP));
	//////////////////////////////////////////////////////////////////////////////////////
	///LEFT TOWER DOWN//////////////////////////////////////

	auto leftTowerDOWN = std::make_unique<RenderItem>();
	auto RooftleftTowerDOWN = std::make_unique<RenderItem>();
	XMMATRIX leftTowerDownWorld = XMMatrixTranslation(-24.5f, 9.50f, -24.5f);
	XMMATRIX RooftleftTowerDOWNWorld = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-24.5f, 22.5f, -24.5f);

	XMStoreFloat4x4(&leftTowerDOWN->World, leftTowerDownWorld);
	XMStoreFloat4x4(&RooftleftTowerDOWN->World, RooftleftTowerDOWNWorld);


	leftTowerDOWN->ObjCBIndex = objCBIndex++;
	RooftleftTowerDOWN->ObjCBIndex = objCBIndex++;

	leftTowerDOWN->Mat = mMaterials["wall"].get();
	RooftleftTowerDOWN->Mat = mMaterials["wall3"].get();


	leftTowerDOWN->Geo = mGeometries["TowerGeo"].get();
	RooftleftTowerDOWN->Geo = mGeometries["TowerTopGeo"].get();
	leftTowerDOWN->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftleftTowerDOWN->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	leftTowerDOWN->IndexCount = leftTowerDOWN->Geo->DrawArgs["Tower"].IndexCount;
	RooftleftTowerDOWN->IndexCount = RooftleftTowerDOWN->Geo->DrawArgs["TowerTop"].IndexCount;


	leftTowerDOWN->StartIndexLocation = leftTowerDOWN->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftleftTowerDOWN->StartIndexLocation = RooftleftTowerDOWN->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	leftTowerDOWN->BaseVertexLocation = leftTowerDOWN->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftleftTowerDOWN->BaseVertexLocation = RooftleftTowerDOWN->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(leftTowerDOWN.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftleftTowerDOWN.get());

	mAllRitems.push_back(std::move(leftTowerDOWN));
	mAllRitems.push_back(std::move(RooftleftTowerDOWN));




	// RIGHT TOWER UP ///////////////////////////////////////////////////////////////////

	auto rightTowerUP = std::make_unique<RenderItem>();
	auto RoofrightTowerUP = std::make_unique<RenderItem>();
	XMMATRIX rightTowerUPWorld = XMMatrixTranslation(24.5f, 9.50f, 24.5f);
	XMMATRIX RooftrightTowerUPWorld = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(24.5f, 22.5f, 24.5f);

	XMStoreFloat4x4(&rightTowerUP->World, rightTowerUPWorld);
	XMStoreFloat4x4(&RoofrightTowerUP->World, RooftrightTowerUPWorld);


	rightTowerUP->ObjCBIndex = objCBIndex++;
	RoofrightTowerUP->ObjCBIndex = objCBIndex++;

	rightTowerUP->Mat = mMaterials["wall"].get();
	RoofrightTowerUP->Mat = mMaterials["wall3"].get();


	rightTowerUP->Geo = mGeometries["TowerGeo"].get();
	RoofrightTowerUP->Geo = mGeometries["TowerTopGeo"].get();
	rightTowerUP->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RoofrightTowerUP->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	rightTowerUP->IndexCount = rightTowerUP->Geo->DrawArgs["Tower"].IndexCount;
	RoofrightTowerUP->IndexCount = RoofrightTowerUP->Geo->DrawArgs["TowerTop"].IndexCount;


	rightTowerUP->StartIndexLocation = rightTowerUP->Geo->DrawArgs["Tower"].StartIndexLocation;
	RoofrightTowerUP->StartIndexLocation = RoofrightTowerUP->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	rightTowerUP->BaseVertexLocation = rightTowerUP->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RoofrightTowerUP->BaseVertexLocation = RoofrightTowerUP->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(rightTowerUP.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RoofrightTowerUP.get());

	mAllRitems.push_back(std::move(rightTowerUP));
	mAllRitems.push_back(std::move(RoofrightTowerUP));




	//RIGHT TOWER DOWN
	auto rightTowerDown = std::make_unique<RenderItem>();
	auto RooftrightTowerDown = std::make_unique<RenderItem>();


	XMMATRIX rightTowerDownWorld = XMMatrixTranslation(24.5f, 9.50f, -24.5f);
	XMMATRIX RooftrightTowerDownWorld = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(24.5f, 22.5f, -24.5f);

	XMStoreFloat4x4(&rightTowerDown->World, rightTowerDownWorld);
	XMStoreFloat4x4(&RooftrightTowerDown->World, RooftrightTowerDownWorld);

	rightTowerDown->ObjCBIndex = objCBIndex++;
	RooftrightTowerDown->ObjCBIndex = objCBIndex++;
	rightTowerDown->Mat = mMaterials["wall"].get();
	RooftrightTowerDown->Mat = mMaterials["wall3"].get();

	rightTowerDown->Geo = mGeometries["TowerGeo"].get();
	RooftrightTowerDown->Geo = mGeometries["TowerTopGeo"].get();

	rightTowerDown->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftrightTowerDown->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	rightTowerDown->IndexCount = rightTowerDown->Geo->DrawArgs["Tower"].IndexCount;
	RooftrightTowerDown->IndexCount = RooftrightTowerDown->Geo->DrawArgs["TowerTop"].IndexCount;


	rightTowerDown->StartIndexLocation = rightTowerDown->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftrightTowerDown->StartIndexLocation = RooftrightTowerDown->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	rightTowerDown->BaseVertexLocation = rightTowerDown->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftrightTowerDown->BaseVertexLocation = RooftrightTowerDown->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(rightTowerDown.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftrightTowerDown.get());



	mAllRitems.push_back(std::move(rightTowerDown));
	mAllRitems.push_back(std::move(RooftrightTowerDown));

	//LEFT TOWER - UP - INNER/////////////////////////////////////
	auto leftTowerUPInner = std::make_unique<RenderItem>();
	auto RooftleftTowerUPInner = std::make_unique<RenderItem>();
	XMMATRIX leftTowerWorldInner = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(-9.5f, 14.0f, +9.5f);
	XMMATRIX RooftleftTowerUPWorldInner = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-9.5f, 32.0f, +9.5f);

	XMStoreFloat4x4(&leftTowerUPInner->World, leftTowerWorldInner);
	XMStoreFloat4x4(&RooftleftTowerUPInner->World, RooftleftTowerUPWorldInner);

	leftTowerUPInner->ObjCBIndex = objCBIndex++;
	RooftleftTowerUPInner->ObjCBIndex = objCBIndex++;

	leftTowerUPInner->Mat = mMaterials["sample1"].get();
	RooftleftTowerUPInner->Mat = mMaterials["wall3"].get();


	leftTowerUPInner->Geo = mGeometries["TowerGeo"].get();
	RooftleftTowerUPInner->Geo = mGeometries["TowerTopGeo"].get();
	leftTowerUPInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftleftTowerUPInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	leftTowerUPInner->IndexCount = leftTowerUPInner->Geo->DrawArgs["Tower"].IndexCount;
	RooftleftTowerUPInner->IndexCount = RooftleftTowerUPInner->Geo->DrawArgs["TowerTop"].IndexCount;


	leftTowerUPInner->StartIndexLocation = leftTowerUPInner->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftleftTowerUPInner->StartIndexLocation = RooftleftTowerUPInner->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	leftTowerUPInner->BaseVertexLocation = leftTowerUPInner->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftleftTowerUPInner->BaseVertexLocation = RooftleftTowerUPInner->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(leftTowerUPInner.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftleftTowerUPInner.get());

	mAllRitems.push_back(std::move(leftTowerUPInner));
	mAllRitems.push_back(std::move(RooftleftTowerUPInner));

	//Right TOWER - UP - INNER/////////////////////////////////////
	auto rightTowerUPInner = std::make_unique<RenderItem>();
	auto RooftrightTowerUPInner = std::make_unique<RenderItem>();
	XMMATRIX rightTowerWorldInner = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(9.5f, 14.0f, +9.5f);
	XMMATRIX RooftrightTowerUPWorldInner = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(9.5f, 32.0f, +9.5f);

	XMStoreFloat4x4(&rightTowerUPInner->World, rightTowerWorldInner);
	XMStoreFloat4x4(&RooftrightTowerUPInner->World, RooftrightTowerUPWorldInner);

	rightTowerUPInner->ObjCBIndex = objCBIndex++;
	RooftrightTowerUPInner->ObjCBIndex = objCBIndex++;

	rightTowerUPInner->Mat = mMaterials["sample1"].get();
	RooftrightTowerUPInner->Mat = mMaterials["wall3"].get();


	rightTowerUPInner->Geo = mGeometries["TowerGeo"].get();
	RooftrightTowerUPInner->Geo = mGeometries["TowerTopGeo"].get();
	rightTowerUPInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftrightTowerUPInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	rightTowerUPInner->IndexCount = rightTowerUPInner->Geo->DrawArgs["Tower"].IndexCount;
	RooftrightTowerUPInner->IndexCount = RooftrightTowerUPInner->Geo->DrawArgs["TowerTop"].IndexCount;


	rightTowerUPInner->StartIndexLocation = rightTowerUPInner->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftrightTowerUPInner->StartIndexLocation = RooftrightTowerUPInner->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	rightTowerUPInner->BaseVertexLocation = rightTowerUPInner->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftrightTowerUPInner->BaseVertexLocation = RooftrightTowerUPInner->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(rightTowerUPInner.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftrightTowerUPInner.get());

	mAllRitems.push_back(std::move(rightTowerUPInner));
	mAllRitems.push_back(std::move(RooftrightTowerUPInner));

	//LEFT TOWER - BOT - INNER/////////////////////////////////////
	auto leftTowerBOTInner = std::make_unique<RenderItem>();
	auto RooftleftTowerBOTInner = std::make_unique<RenderItem>();
	XMMATRIX leftTowerWorldInner1 = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(-9.5f, 14.0f, -9.5f);
	XMMATRIX RooftleftTowerBOTWorldInner = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-9.5f, 32.0f, -9.5f);

	XMStoreFloat4x4(&leftTowerBOTInner->World, leftTowerWorldInner1);
	XMStoreFloat4x4(&RooftleftTowerBOTInner->World, RooftleftTowerBOTWorldInner);

	leftTowerBOTInner->ObjCBIndex = objCBIndex++;
	RooftleftTowerBOTInner->ObjCBIndex = objCBIndex++;

	leftTowerBOTInner->Mat = mMaterials["sample1"].get();
	RooftleftTowerBOTInner->Mat = mMaterials["wall3"].get();


	leftTowerBOTInner->Geo = mGeometries["TowerGeo"].get();
	RooftleftTowerBOTInner->Geo = mGeometries["TowerTopGeo"].get();
	leftTowerBOTInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftleftTowerBOTInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	leftTowerBOTInner->IndexCount = leftTowerBOTInner->Geo->DrawArgs["Tower"].IndexCount;
	RooftleftTowerBOTInner->IndexCount = RooftleftTowerBOTInner->Geo->DrawArgs["TowerTop"].IndexCount;


	leftTowerBOTInner->StartIndexLocation = leftTowerBOTInner->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftleftTowerBOTInner->StartIndexLocation = RooftleftTowerBOTInner->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	leftTowerBOTInner->BaseVertexLocation = leftTowerBOTInner->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftleftTowerBOTInner->BaseVertexLocation = RooftleftTowerBOTInner->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(leftTowerBOTInner.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftleftTowerBOTInner.get());

	mAllRitems.push_back(std::move(leftTowerBOTInner));
	mAllRitems.push_back(std::move(RooftleftTowerBOTInner));

	//Right TOWER - BOT - INNER/////////////////////////////////////
	auto rightTowerBOTInner = std::make_unique<RenderItem>();
	auto RooftrightTowerBOTInner = std::make_unique<RenderItem>();
	XMMATRIX rightTowerWorldInner1 = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(9.5f, 14.0f, -9.5f);
	XMMATRIX RooftrightTowerBOTWorldInner = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(9.5f, 32.0f, -9.5f);

	XMStoreFloat4x4(&rightTowerBOTInner->World, rightTowerWorldInner1);
	XMStoreFloat4x4(&RooftrightTowerBOTInner->World, RooftrightTowerBOTWorldInner);

	rightTowerBOTInner->ObjCBIndex = objCBIndex++;
	RooftrightTowerBOTInner->ObjCBIndex = objCBIndex++;

	rightTowerBOTInner->Mat = mMaterials["sample1"].get();
	RooftrightTowerBOTInner->Mat = mMaterials["wall3"].get();


	rightTowerBOTInner->Geo = mGeometries["TowerGeo"].get();
	RooftrightTowerBOTInner->Geo = mGeometries["TowerTopGeo"].get();
	rightTowerBOTInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RooftrightTowerBOTInner->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	rightTowerBOTInner->IndexCount = rightTowerBOTInner->Geo->DrawArgs["Tower"].IndexCount;
	RooftrightTowerBOTInner->IndexCount = RooftrightTowerBOTInner->Geo->DrawArgs["TowerTop"].IndexCount;


	rightTowerBOTInner->StartIndexLocation = rightTowerBOTInner->Geo->DrawArgs["Tower"].StartIndexLocation;
	RooftrightTowerBOTInner->StartIndexLocation = RooftrightTowerBOTInner->Geo->DrawArgs["TowerTop"].StartIndexLocation;

	rightTowerBOTInner->BaseVertexLocation = rightTowerBOTInner->Geo->DrawArgs["Tower"].BaseVertexLocation;
	RooftrightTowerBOTInner->BaseVertexLocation = RooftrightTowerBOTInner->Geo->DrawArgs["TowerTop"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(rightTowerBOTInner.get());
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(RooftrightTowerBOTInner.get());

	mAllRitems.push_back(std::move(rightTowerBOTInner));
	mAllRitems.push_back(std::move(RooftrightTowerBOTInner));
	

}
void TreeBillboardsApp::BuildRenderGate() 
{
	UINT objCBIndex = 24;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////																									
	auto mainGate = std::make_unique<RenderItem>();																		  //
																														  //
	XMStoreFloat4x4(&mainGate->World, XMMatrixScaling(14.0f, 14.8f, 3.0f) * XMMatrixTranslation(0.0f, 7.1f, -25.0f));    //
	mainGate->ObjCBIndex = objCBIndex++;//
	mainGate->Mat = mMaterials["wall3"].get();
	mainGate->Geo = mGeometries["GateGeo"].get();
	mainGate->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mainGate->IndexCount = mainGate->Geo->DrawArgs["Gate"].IndexCount;
	mainGate->StartIndexLocation = mainGate->Geo->DrawArgs["Gate"].StartIndexLocation;									   //
	mainGate->BaseVertexLocation = mainGate->Geo->DrawArgs["Gate"].BaseVertexLocation;									   //
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(mainGate.get());												   //
	mAllRitems.push_back(std::move(mainGate));																			   //
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
																														   //
	auto separationGate = std::make_unique<RenderItem>();																   //													  //
	XMStoreFloat4x4(&separationGate->World, XMMatrixScaling(0.1f, 14.8f, 3.2f) * XMMatrixTranslation(0.0f, 7.1f, -25.0f));//
																														   //
	separationGate->ObjCBIndex = objCBIndex++;//
	separationGate->Mat = mMaterials["wirefence"].get();
	separationGate->Geo = mGeometries["GateGeo"].get();
	separationGate->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	separationGate->IndexCount = separationGate->Geo->DrawArgs["Gate"].IndexCount;
	separationGate->StartIndexLocation = separationGate->Geo->DrawArgs["Gate"].StartIndexLocation;							//
	separationGate->BaseVertexLocation = separationGate->Geo->DrawArgs["Gate"].BaseVertexLocation;							//
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(separationGate.get());												//
	mAllRitems.push_back(std::move(separationGate));																		//
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 

	auto boxRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem2->World, XMMatrixScaling(0.50f, 1.2f, 0.50f) * XMMatrixTranslation(0.0f, 11.5f, 0.0f));
	XMStoreFloat4x4(&boxRitem2->TexTransform, XMMatrixScaling(3.0f, 2.0f, 1.0f));
	
	boxRitem2->ObjCBIndex = objCBIndex++;
	boxRitem2->Mat = mMaterials["sample1"].get();
	boxRitem2->Geo = mGeometries["xGeo"].get();
	boxRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["x"].IndexCount;
	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["x"].StartIndexLocation;
	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["x"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem2.get());
	mAllRitems.push_back(std::move(boxRitem2));



	// 
	// 
	// 
	////MERLONS===============================================================MERLONS====================================///
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////																									
	for (int i = 0; i < 88; ++i)																												//
	{																																			//
																																				//
		auto Merlons = std::make_unique<RenderItem>();																	 							//
		if (i < 22)																																//
		{																																		//
			XMStoreFloat4x4(&Merlons->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-21.0f + i * 2.0f, 16.0f, -25.0f));				//
		}																																		//
		else if (i < 44)																															//
		{																																		//
			XMStoreFloat4x4(&Merlons->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-21.0f + (i - 22) * 2.0f, 16.0f, 25.0f));		//
		}																																		//
		else if (i < 66)																														//
		{																																		//
			XMStoreFloat4x4(&Merlons->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-25.0f, 16.0f, -21.0f + (i - 44) * 2.0f));		//
		}																																		//
		else																																	// Build wall's top
		{																																		//
			XMStoreFloat4x4(&Merlons->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(25.0f, 16.0f, -21.0f + (i - 66) * 2.0f));		//
		}

		Merlons->ObjCBIndex = objCBIndex++;//
		Merlons->Mat = mMaterials["wall2"].get();
		Merlons->Geo = mGeometries["MerlonGeo"].get();
		Merlons->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		Merlons->IndexCount = Merlons->Geo->DrawArgs["Merlon"].IndexCount;
		Merlons->StartIndexLocation = Merlons->Geo->DrawArgs["Merlon"].StartIndexLocation;							//
		Merlons->BaseVertexLocation = Merlons->Geo->DrawArgs["Merlon"].BaseVertexLocation;							//
		mRitemLayer[(int)RenderLayer::AlphaTested].push_back(Merlons.get());												//
		mAllRitems.push_back(std::move(Merlons));
	}

}

void TreeBillboardsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		//step3
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TreeBillboardsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

float TreeBillboardsApp::GetHillsHeight(float x, float z)const
{
    return 0.3f*(z*sinf(0.1f*x) + x*cosf(0.1f*z));
}

XMFLOAT3 TreeBillboardsApp::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
        1.0f,
        -0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}