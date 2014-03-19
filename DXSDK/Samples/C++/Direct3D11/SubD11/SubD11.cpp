//--------------------------------------------------------------------------------------
// File: SubD11.cpp
//
// This sample shows an implementation of Charles Loop's and Scott Schaefer's Approximate
// Catmull-Clark subdvision paper (http://research.microsoft.com/~cloop/msrtr-2007-44.pdf)
//
// Special thanks to Charles Loop and Peter-Pike Sloan for implementation details.
// Special thanks to Bay Raitt for the monsterfrog and bigguy models.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include "SubDMesh.h"

#define MAX_BUMP 3000                           // Maximum bump amount * 1000 (for UI slider)
#define MAX_DIVS 32                             // Maximum divisions of a patch per side (about 2048 triangles)

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                   // A model viewing camera
CDXUTDirectionWidget                g_LightControl;
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // manages the 3D   
CDXUTDialog                         g_SampleUI;                 // dialog for sample specific controls

// Resources
CDXUTTextHelper*                    g_pTxtHelper = NULL;

ID3D11InputLayout*					g_pPatchLayout = NULL;

ID3D11VertexShader*					g_pSkinningVS = NULL;
ID3D11HullShader*					g_pSubDToBezierHS = NULL;
ID3D11DomainShader*					g_pBezierEvalDS = NULL;
ID3D11PixelShader*					g_pSmoothPS = NULL;
ID3D11PixelShader*					g_pSolidColorPS = NULL;

struct CB_TANGENT_STENCIL_CONSTANTS
{
    float TanM[MAX_VALENCE][64][4];	// Tangent patch stencils precomputed by the application
    float fCi[16][4];				// Valence coefficients precomputed by the application
};

#define MAX_BONE_MATRICES 80
struct CB_PER_MESH_CONSTANTS
{
    D3DXMATRIX mConstBoneWorld[MAX_BONE_MATRICES];
};

struct CB_PER_FRAME_CONSTANTS
{
    D3DXMATRIX mViewProjection;
    D3DXVECTOR3 vCameraPosWorld;
    float fTessellationFactor;
    float fDisplacementHeight;
    DWORD dwPadding[3];
};

ID3D11RasterizerState*              g_pRasterizerStateSolid = NULL;
ID3D11RasterizerState*              g_pRasterizerStateWireframe = NULL;
ID3D11SamplerState*                 g_pSamplerStateHeightMap = NULL;
ID3D11SamplerState*                 g_pSamplerStateNormalMap = NULL;

ID3D11Buffer*						g_pcbTangentStencilConstants = NULL;
ID3D11Buffer*						g_pcbPerMesh = NULL;
ID3D11Buffer*						g_pcbPerFrame = NULL;

UINT								g_iBindTangentStencilConstants = 0;
UINT								g_iBindPerMesh = 1;
UINT								g_iBindPerFrame = 2;
UINT								g_iBindValencePrefixBuffer = 0;

// Control variables
int                                 g_iSubdivs = 2;                         // Startup subdivisions per side
bool                                g_bDrawWires = false;                   // Draw the mesh with wireframe overlay
bool                                g_bSeparatePasses = true;               // Do a separate pass for regular and extraordinary patches
bool                                g_bConvertEveryFrame = true;            // Convert from subd to bezier every frame
bool                                g_bAttenuateBasedOnDistance = true;     // Attenuate the tesselation level based upon distance to the camera
float                               g_fDisplacementHeight = 0.5f;           // The height amount for displacement mapping
const float                         g_fFarPlane = 500.0f;                   // Distance to the far plane

// Mesh structures
struct SUBDMESH_DESC
{
    WCHAR       m_szFileName[MAX_PATH];
    WCHAR       m_szAnimationFileName[MAX_PATH];
    D3DXVECTOR3 m_vPosition;
    FLOAT       m_fScale;
};

SUBDMESH_DESC g_MeshDesc[] =
{
    { 
        L"SubD10\\animatedhead.sdkmesh",      
        L"SubD10\\animatedhead.sdkmesh_anim",      
        D3DXVECTOR3(  0.0f,  0.0f,  0.0f ), 
        0.1f 
    },
};

CSubDMesh g_SubDMesh[ ARRAYSIZE( g_MeshDesc ) ];
const UINT g_iNumSubDMeshes = ARRAYSIZE( g_MeshDesc );

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN      1
#define IDC_TOGGLEREF             3
#define IDC_CHANGEDEVICE          4

#define IDC_PATCH_SUBDIVS         5
#define IDC_PATCH_SUBDIVS_STATIC  6
#define IDC_BUMP_HEIGHT	          7
#define IDC_BUMP_HEIGHT_STATIC    8
#define IDC_TOGGLE_LINES          9
#define IDC_TECH_STATIC           10
#define IDC_TECH                  11
#define IDC_SEPARATE_PASSES       12
#define IDC_CONVERT_PER_FRAME     13
#define IDC_DIST_ATTEN            14
#define IDC_SKIN                  15

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );


void InitApp();
void RenderText();
UINT SetPatchConstants( ID3D11Device* pd3dDevice, UINT iSubdivs );
void UpdateSubDPatches( ID3D11Device* pd3dDevice, CSubDMesh* pMesh );
void ConvertFromSubDToBezier( ID3D11Device* pd3dDevice, CSubDMesh* pMesh );
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice );
void FillTables( ID3D11DeviceContext* pd3dDeviceContext );

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif



    // DXUT will create and use the best device (either D3D10 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, L"-forceref" ); // Parse the command line, show msgboxes on error, and an extra cmd line param to force REF for now
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SubD11" );
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0,  true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_LightControl.SetLightDirection( D3DXVECTOR3( 0, 0, -1 ) );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 20;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    WCHAR sz[100];
    iY += 24;
    swprintf_s( sz, L"Patch Divisions: %d", g_iSubdivs );
    g_SampleUI.AddStatic( IDC_PATCH_SUBDIVS_STATIC, sz, 20, iY += 26, 150, 22 );
    g_SampleUI.AddSlider( IDC_PATCH_SUBDIVS, 50, iY += 24, 100, 22, 1, MAX_DIVS - 1, g_iSubdivs );

    swprintf_s( sz, L"BumpHeight: %.4f", g_fDisplacementHeight );
    g_SampleUI.AddStatic( IDC_BUMP_HEIGHT_STATIC, sz, 20, iY += 26, 150, 22 );
    g_SampleUI.AddSlider( IDC_BUMP_HEIGHT, 50, iY += 24, 100, 22, 0, MAX_BUMP, ( int )( 1000.0f * g_fDisplacementHeight ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_LINES, L"Toggle Wires", 20, iY += 26, 150, 22, g_bDrawWires );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 1.0f, 1.5f, -3.5f );
    D3DXVECTOR3 vecAt ( 0.0f, 1.5f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        const SUBDMESH_DESC* pMeshDesc = &g_MeshDesc[i];

        // Calculate our transformation matrix
        D3DXMATRIX mTranslation;
        D3DXMatrixTranslation( &mTranslation, pMeshDesc->m_vPosition.x, pMeshDesc->m_vPosition.y, pMeshDesc->m_vPosition.z );

        D3DXMATRIX mScale;
        D3DXMatrixScaling( &mScale, pMeshDesc->m_fScale, pMeshDesc->m_fScale, pMeshDesc->m_fScale );
        D3DXMATRIX mWorld = mScale * mTranslation;

        g_SubDMesh[i].Update( &mWorld, fElapsedTime );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
            // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;

            // Custom app controls
        case IDC_PATCH_SUBDIVS:
        {
            g_iSubdivs = g_SampleUI.GetSlider( IDC_PATCH_SUBDIVS )->GetValue();

            WCHAR sz[100];
            swprintf_s( sz, L"Patch Divisions: %d", g_iSubdivs );
            g_SampleUI.GetStatic( IDC_PATCH_SUBDIVS_STATIC )->SetText( sz );
        }
            break;
        case IDC_BUMP_HEIGHT:
        {
            g_fDisplacementHeight = ( float )g_SampleUI.GetSlider( IDC_BUMP_HEIGHT )->GetValue() / 1000.0f;

            WCHAR sz[100];
            swprintf_s( sz, L"BumpHeight: %.4f", g_fDisplacementHeight );
            g_SampleUI.GetStatic( IDC_BUMP_HEIGHT_STATIC )->SetText( sz );
        }
            break;
        case IDC_TOGGLE_LINES:
        {
            g_bDrawWires = g_SampleUI.GetCheckBox( IDC_TOGGLE_LINES )->GetChecked();
        }
            break;
        case IDC_SEPARATE_PASSES:
        {
            g_bSeparatePasses = !g_bSeparatePasses;
        }
            break;
        case IDC_CONVERT_PER_FRAME:
        {
            g_bConvertEveryFrame = !g_bConvertEveryFrame;
        }
            break;
        case IDC_DIST_ATTEN:
        {
            g_bAttenuateBasedOnDistance = !g_bAttenuateBasedOnDistance;
        }
            break;

    }

}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_TANGENT_STENCIL_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbTangentStencilConstants ) );

    Desc.ByteWidth = sizeof( CB_PER_MESH_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPerMesh ) );

    Desc.ByteWidth = sizeof( CB_PER_FRAME_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPerFrame ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// Use this until D3DX11 comes online and we get some compilation helpers
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    // open the file
    HANDLE hFile = CreateFile( str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    // Get the file size
    LARGE_INTEGER FileSize;
    GetFileSizeEx( hFile, &FileSize );

    // create enough space for the file data
    BYTE* pFileData = new BYTE[ FileSize.LowPart ];
    if( !pFileData )
        return E_OUTOFMEMORY;

    // read the data in
    DWORD BytesRead;
    if( !ReadFile( hFile, pFileData, FileSize.LowPart, &BytesRead, NULL ) )
        return E_FAIL; 

    CloseHandle( hFile );

    // Compile the shader
    ID3DBlob* pErrorBlob;
    hr = D3DCompile( pFileData, FileSize.LowPart, "none", NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Compile shaders
    ID3DBlob* pBlobVS = NULL;
    ID3DBlob* pBlobHS = NULL;
    ID3DBlob* pBlobDS = NULL;
    ID3DBlob* pBlobPS = NULL;
    ID3DBlob* pBlobPSSolid = NULL;

    V_RETURN( CompileShaderFromFile( L"SubD11.hlsl", "SkinningVS",	   "vs_5_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SubD11.hlsl", "SubDToBezierHS", "hs_5_0", &pBlobHS ) );
    V_RETURN( CompileShaderFromFile( L"SubD11.hlsl", "BezierEvalDS",   "ds_5_0", &pBlobDS ) );
    V_RETURN( CompileShaderFromFile( L"SubD11.hlsl", "SmoothPS",	   "ps_5_0", &pBlobPS ) );
    V_RETURN( CompileShaderFromFile( L"SubD11.hlsl", "SolidColorPS",   "ps_5_0", &pBlobPSSolid ) );

    // Create shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pSkinningVS ) );
    V_RETURN( pd3dDevice->CreateHullShader( pBlobHS->GetBufferPointer(), pBlobHS->GetBufferSize(), NULL, &g_pSubDToBezierHS ) );
    V_RETURN( pd3dDevice->CreateDomainShader( pBlobDS->GetBufferPointer(), pBlobDS->GetBufferSize(), NULL, &g_pBezierEvalDS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pSmoothPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPSSolid->GetBufferPointer(), pBlobPSSolid->GetBufferSize(), NULL, &g_pSolidColorPS ) );
    
    // Create our vertex input layout - this matches the SUBD_CONTROL_POINT structure
    const D3D11_INPUT_ELEMENT_DESC patchlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES",    0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( patchlayout, ARRAYSIZE( patchlayout ), pBlobVS->GetBufferPointer(),
                                             pBlobVS->GetBufferSize(), &g_pPatchLayout ) );

    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobHS );
    SAFE_RELEASE( pBlobDS );
    SAFE_RELEASE( pBlobPS );
    SAFE_RELEASE( pBlobPSSolid );
    
    // Create constant buffers
    V_RETURN( CreateConstantBuffers( pd3dDevice ) );

    // Fill our helper/temporary tables
    FillTables( pd3dImmediateContext );
    
    // Load meshes	
    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        // Load the subd mesh from a SDKMESH file
        V_RETURN( g_SubDMesh[i].LoadSubDFromSDKMesh( pd3dDevice, g_MeshDesc[i].m_szFileName, g_MeshDesc[i].m_szAnimationFileName ) );
    }

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;
    RasterDesc.DepthClipEnable = TRUE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );

    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );

    // Create sampler state for heightmap and normal map
    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( D3D11_SAMPLER_DESC ) );
    SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SSDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy = 16;
    SSDesc.MinLOD = 0;
    SSDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateNormalMap ) );

    SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateHeightMap ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 2.0f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Use the gpu to convert from subds to cubic bezier patches using stream out
//--------------------------------------------------------------------------------------
void RenderSubDMesh( ID3D11DeviceContext* pd3dDeviceContext, CSubDMesh* pMesh, SUBDMESH_DESC* pMeshDesc, ID3D11PixelShader* pPixelShader )
{

    // Bind all of the CBs
    pd3dDeviceContext->HSSetConstantBuffers( g_iBindTangentStencilConstants, 1, &g_pcbTangentStencilConstants );
    pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );

    // Set the shaders
    pd3dDeviceContext->VSSetShader( g_pSkinningVS, NULL, 0 );
    pd3dDeviceContext->HSSetShader( g_pSubDToBezierHS, NULL, 0 );
    pd3dDeviceContext->DSSetShader( g_pBezierEvalDS, NULL, 0 );
    pd3dDeviceContext->GSSetShader( NULL, NULL, 0 );
    pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );

    // Set the heightmap sampler state
    pd3dDeviceContext->DSSetSamplers( 0, 1, &g_pSamplerStateHeightMap );
    pd3dDeviceContext->PSSetSamplers( 0, 1, &g_pSamplerStateNormalMap );

    // Set the input layout
    pd3dDeviceContext->IASetInputLayout( g_pPatchLayout );

    int PieceCount = pMesh->GetNumMeshPieces();
    for( int i = 0; i < PieceCount; ++i )
    {
        // Per frame cb update
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( g_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_PER_MESH_CONSTANTS* pData = ( CB_PER_MESH_CONSTANTS* )MappedResource.pData;

        int NumTransforms = pMesh->GetNumInfluences( i );
        assert( NumTransforms <= MAX_BONE_MATRICES );
        for( int j = 0; j < NumTransforms; ++j )
        {
            D3DXMatrixTranspose( &pData->mConstBoneWorld[j], pMesh->GetInfluenceMatrix( i, j ) );
        }
        if( NumTransforms == 0 )
        {
            D3DXMATRIX matTransform;
            pMesh->GetMeshPieceTransform( i, &matTransform );
            D3DXMatrixTranspose( &pData->mConstBoneWorld[0], &matTransform );
        }
        pd3dDeviceContext->Unmap( g_pcbPerMesh, 0 );
        pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerMesh, 1, &g_pcbPerMesh );

        pMesh->RenderMeshPiece( pd3dDeviceContext, i );
    }

}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // WVP
    D3DXMATRIX mViewProjection;
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
    D3DXMATRIX mView = *g_Camera.GetViewMatrix();

    mViewProjection = mView * mProj;

    // Update per-frame variables
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_PER_FRAME_CONSTANTS* pData = ( CB_PER_FRAME_CONSTANTS* )MappedResource.pData;

    D3DXMatrixTranspose( &pData->mViewProjection, &mViewProjection );
    pData->vCameraPosWorld = *( g_Camera.GetEyePt() );
    pData->fTessellationFactor = (float)g_iSubdivs;
    pData->fDisplacementHeight = g_fDisplacementHeight;

    pd3dImmediateContext->Unmap( g_pcbPerFrame, 0 );
    
    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.05f, 0.05f, 0.05f, 0.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set state for solid rendering
    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

    // Render the meshes
    for( UINT i=0; i<g_iNumSubDMeshes; i++ )
    {
        RenderSubDMesh( pd3dImmediateContext, &g_SubDMesh[i], &g_MeshDesc[i], g_pSmoothPS );
    }

    // Optionally draw overlay wireframe
    if( g_bDrawWires )
    {
        pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe );
        // Render the meshes
        for( UINT i=0; i<g_iNumSubDMeshes; i++ )
        {
            RenderSubDMesh( pd3dImmediateContext, &g_SubDMesh[i], &g_MeshDesc[i], g_pSolidColorPS );
        }
        pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );
    }

    // Render the HUD
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pPatchLayout );
    SAFE_RELEASE( g_pcbTangentStencilConstants );
    SAFE_RELEASE( g_pcbPerMesh );
    SAFE_RELEASE( g_pcbPerFrame );

    SAFE_RELEASE( g_pSkinningVS );
    SAFE_RELEASE( g_pSubDToBezierHS );
    SAFE_RELEASE( g_pBezierEvalDS );
    SAFE_RELEASE( g_pSmoothPS );
    SAFE_RELEASE( g_pSolidColorPS );

    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pSamplerStateHeightMap );
    SAFE_RELEASE( g_pSamplerStateNormalMap );

    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        g_SubDMesh[i].Destroy();
    }
}

//--------------------------------------------------------------------------------------
// Fill the TanM and Ci precalculated tables.  This function precalculates part of the
// stencils (weights) used when calculating UV patches.  We precalculate a lot of the
// values here and just pass them in as shader constants.
//--------------------------------------------------------------------------------------
void FillTables( ID3D11DeviceContext* pd3dDeviceContext )
{
    // Per frame cb update
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dDeviceContext->Map( g_pcbTangentStencilConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_TANGENT_STENCIL_CONSTANTS* pData = ( CB_TANGENT_STENCIL_CONSTANTS* )MappedResource.pData;
    
    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        int a = 0;
        float CosfPIV = cosf( D3DX_PI / v );
        float VSqrtTerm = ( v * sqrtf( 4.0f + CosfPIV * CosfPIV ) );

        for( UINT i = 0; i < 32; i++ )
        {
            pData->TanM[v][i * 2 + a][0] = ( ( 1.0f / v ) + CosfPIV / VSqrtTerm ) * cosf( ( 2 * D3DX_PI * i ) / ( float )v );
        }
        a = 1;
        for( UINT i = 0; i < 32; i++ )
        {
            pData->TanM[v][i * 2 + a][0] = ( 1.0f / VSqrtTerm ) * cosf( ( 2 * D3DX_PI * i + D3DX_PI ) / ( float )v );
        }
    }

    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        pData->fCi[v][0] = cosf( ( 2.0f * D3DX_PI ) / ( v + 3.0f ) );
    }

    // Constants
    pd3dDeviceContext->Unmap( g_pcbTangentStencilConstants, 0 );
}