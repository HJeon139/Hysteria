//--------------------------------------------------------------------------------------
// File: SubDMesh.cpp
//
// This class encapsulates the mesh loading and housekeeping functions for a SubDMesh.
// The mesh loads preprocessed SDKMESH files from disk and stages them for rendering.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SubDMesh.h"
#include "sdkmisc.h"
#include "DXUTRes.h"

ID3D11Texture2D* g_pDefaultDiffuseTexture = NULL;
ID3D11Texture2D* g_pDefaultNormalTexture = NULL;
ID3D11Texture2D* g_pDefaultSpecularTexture = NULL;

ID3D11ShaderResourceView* g_pDefaultDiffuseSRV = NULL;
ID3D11ShaderResourceView* g_pDefaultNormalSRV = NULL;
ID3D11ShaderResourceView* g_pDefaultSpecularSRV = NULL;

struct CB_PER_SUBSET_CONSTANTS
{
    int m_iPatchStartIndex;

    DWORD m_Padding[3];
};

static const UINT g_iBindPerSubset = 3;

//--------------------------------------------------------------------------------------
// Creates a 1x1 uncompressed texture containing the specified color.
//--------------------------------------------------------------------------------------
VOID CreateSolidTexture( ID3D11Device* pd3dDevice, DWORD ColorRGBA, ID3D11Texture2D** ppTexture2D, ID3D11ShaderResourceView** ppSRV )
{
    D3D11_TEXTURE2D_DESC Tex2DDesc = { 0 };
    Tex2DDesc.Width = 1;
    Tex2DDesc.Height = 1;
    Tex2DDesc.ArraySize = 1;
    Tex2DDesc.SampleDesc.Count = 1;
    Tex2DDesc.SampleDesc.Quality = 0;
    Tex2DDesc.MipLevels = 1;
    Tex2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Tex2DDesc.Usage = D3D11_USAGE_DEFAULT;
    Tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA TexData = { 0 };
    TexData.pSysMem = &ColorRGBA;
    TexData.SysMemPitch = 4;
    TexData.SysMemSlicePitch = 4;

    HRESULT hr = pd3dDevice->CreateTexture2D( &Tex2DDesc, &TexData, ppTexture2D );
    assert( SUCCEEDED( hr ) );

    hr = pd3dDevice->CreateShaderResourceView( *ppTexture2D, NULL, ppSRV );
    assert( SUCCEEDED( hr ) );
}

//--------------------------------------------------------------------------------------
// Creates three default textures to be used to replace missing content in the mesh file.
//--------------------------------------------------------------------------------------
VOID CreateDefaultTextures( ID3D11Device* pd3dDevice )
{
    if( g_pDefaultDiffuseTexture == NULL )
    {
        CreateSolidTexture( pd3dDevice, 0xFF808080, &g_pDefaultDiffuseTexture, &g_pDefaultDiffuseSRV );
    }
    if( g_pDefaultNormalTexture == NULL )
    {
        CreateSolidTexture( pd3dDevice, 0x80FF8080, &g_pDefaultNormalTexture, &g_pDefaultNormalSRV );
    }
    if( g_pDefaultSpecularTexture == NULL )
    {
        CreateSolidTexture( pd3dDevice, 0xFF000000, &g_pDefaultSpecularTexture, &g_pDefaultSpecularSRV );
    }
}

//--------------------------------------------------------------------------------------
// This callback is used by the SDKMESH loader to create vertex buffers.  The callback
// creates each buffer as a shader resource, so it can be bound as a shader resource as
// well as a VB.
//--------------------------------------------------------------------------------------
VOID CALLBACK CreateVertexBufferAndShaderResource( ID3D11Device* pDev, ID3D11Buffer** ppBuffer,
                                                   D3D11_BUFFER_DESC BufferDesc, void* pData, void* pContext )
{
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pData;
    HRESULT hr = pDev->CreateBuffer( &BufferDesc, &InitData, ppBuffer );
    assert( SUCCEEDED( hr ) );
    hr;
}

//--------------------------------------------------------------------------------------
// Loads a specially constructed SDKMESH file from disk.  This SDKMESH file contains a
// preprocessed Catmull-Clark subdivision surface, complete with topology and adjacency
// data, as well as the typical mesh vertex data.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::LoadSubDFromSDKMesh( ID3D11Device* pd3dDevice, const WCHAR* strFileName, const WCHAR* strAnimationFileName )
{
    WCHAR wstr[MAX_PATH];
    HRESULT hr;

    // Find the file
    V_RETURN( DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strFileName ) );

    SDKMESH_CALLBACKS11 SubDLoaderCallbacks = { 0 };
    SubDLoaderCallbacks.pCreateVertexBuffer = (LPCREATEVERTEXBUFFER11)CreateVertexBufferAndShaderResource;

    m_pMeshFile = new CDXUTSDKMesh();
    assert( m_pMeshFile != NULL );

    // Load the file
    V_RETURN( m_pMeshFile->Create( pd3dDevice, wstr, false, &SubDLoaderCallbacks ) );

    // Load the animation file
    if( wcslen( strAnimationFileName ) > 0 )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strAnimationFileName ) );
        V_RETURN( m_pMeshFile->LoadAnimation( wstr ) );
    }

    UINT MeshCount = m_pMeshFile->GetNumMeshes();

    if( MeshCount == 0 )
        return E_FAIL;

    const UINT FrameCount = m_pMeshFile->GetNumFrames();

    // Load mesh pieces
    for( UINT i = 0; i < MeshCount; ++i )
    {
        SDKMESH_MESH* pMesh = m_pMeshFile->GetMesh( i );
        assert( pMesh != NULL );

        // SubD meshes have 2 vertex buffers: a control point VB and a patch data VB
        assert( pMesh->NumVertexBuffers == 2 );
        // Make sure the control point VB has the correct stride
        assert( m_pMeshFile->GetVertexStride( i, 0 ) == sizeof( SUBD_CONTROL_POINT ) );
        // Make sure we have at least one subset
        assert( m_pMeshFile->GetNumSubsets( i ) > 0 );
        // Make sure the first subset is made up of quad patches
        assert( m_pMeshFile->GetSubset( i, 0 )->PrimitiveType == PT_QUAD_PATCH_LIST );
        // Make sure the IB is a multiple of the max point size
        assert( m_pMeshFile->GetNumIndices( i ) % MAX_EXTRAORDINARY_POINTS == 0 );

        // Create a new mesh piece and fill it in with all of the buffer pointers
        MeshPiece* pMeshPiece = new MeshPiece;
        ZeroMemory( pMeshPiece, sizeof( MeshPiece ) );
        pMeshPiece->m_pMesh = pMesh;
        pMeshPiece->m_MeshIndex = i;

        pMeshPiece->m_pExtraordinaryPatchIB = m_pMeshFile->GetIB11( pMesh->IndexBuffer );
        pMeshPiece->m_pControlPointVB = m_pMeshFile->GetVB11( i, 0 );
        pMeshPiece->m_pPerPatchDataVB = m_pMeshFile->GetVB11( i, 1 );

        INT iNumPatches = (int)m_pMeshFile->GetNumIndices( i ) / MAX_EXTRAORDINARY_POINTS;
        pMeshPiece->m_iPatchCount = iNumPatches;

        //
        // Create a SRV for the per-patch data
        //
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = iNumPatches * 2;

        V_RETURN( pd3dDevice->CreateShaderResourceView( pMeshPiece->m_pPerPatchDataVB, &SRVDesc, &pMeshPiece->m_pPerPatchDataSRV ) );

        pMeshPiece->m_vCenter = pMesh->BoundingBoxCenter;
        pMeshPiece->m_vExtents = pMesh->BoundingBoxExtents;

        // Find frame that corresponds to this mesh
        pMeshPiece->m_iFrameIndex = -1;
        for( UINT j = 0; j < FrameCount; ++j )
        {
            SDKMESH_FRAME* pFrame = m_pMeshFile->GetFrame( j );
            if( pFrame->Mesh == pMeshPiece->m_MeshIndex )
            {
                pMeshPiece->m_iFrameIndex = (INT)j;
            }
        }

        m_MeshPieces.Add( pMeshPiece );
    }

    CreateDefaultTextures( pd3dDevice );

    // Fill in empty textures in materials with default textures
    int MaterialCount = m_pMeshFile->GetNumMaterials();
    for( int i = 0; i < MaterialCount; ++i )
    {
        SDKMESH_MATERIAL* pMaterial = m_pMeshFile->GetMaterial( i );

        if( pMaterial->pDiffuseRV11 == NULL || pMaterial->pDiffuseRV11 == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
        {
            pMaterial->pDiffuseRV11 = g_pDefaultDiffuseSRV;
        }

        if( pMaterial->pNormalRV11 == NULL || pMaterial->pNormalRV11 == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
        {
            pMaterial->pNormalRV11 = g_pDefaultNormalSRV;
        }

        if( pMaterial->pSpecularRV11 == NULL || pMaterial->pSpecularRV11 == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
        {
            pMaterial->pSpecularRV11 = g_pDefaultSpecularSRV;
        }
    }

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_PER_SUBSET_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pPerSubsetCB ) );

    // Create bind pose
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pMeshFile->TransformBindPose( &matIdentity );

    return S_OK;
}

//--------------------------------------------------------------------------------------
CSubDMesh::~CSubDMesh()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
void CSubDMesh::Update( D3DXMATRIX* pWorld, double fTime )
{
    m_pMeshFile->TransformMesh( pWorld, fTime );
}

//--------------------------------------------------------------------------------------
// Renders a single mesh from the SDKMESH data.  Each mesh "piece" is a separate mesh
// within the file, with its own VB and IB buffers.
//--------------------------------------------------------------------------------------
void CSubDMesh::RenderMeshPiece( ID3D11DeviceContext* pd3dDeviceContext, int PieceIndex )
{
    MeshPiece* pPiece = m_MeshPieces[PieceIndex];
    assert( pPiece != NULL );

    // Set the input assembler
    pd3dDeviceContext->IASetIndexBuffer( pPiece->m_pExtraordinaryPatchIB, DXGI_FORMAT_R32_UINT, 0 );
    UINT Stride = sizeof( SUBD_CONTROL_POINT );
    UINT Offset = 0;
    pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pPiece->m_pControlPointVB, &Stride, &Offset );

    pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST );

    // Bind the per-patch data
    pd3dDeviceContext->HSSetShaderResources( 0, 1, &pPiece->m_pPerPatchDataSRV );

    // Loop through the mesh subsets
    int SubsetCount = m_pMeshFile->GetNumSubsets( pPiece->m_MeshIndex );
    for( int i = 0; i < SubsetCount; ++i )
    {
        SDKMESH_SUBSET* pSubset = m_pMeshFile->GetSubset( pPiece->m_MeshIndex, i );
        if( pSubset->PrimitiveType != PT_QUAD_PATCH_LIST )
            continue;

        // Set per-subset constant buffer, so the hull shader references the proper index in the per-patch data
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( m_pPerSubsetCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_PER_SUBSET_CONSTANTS* pData = (CB_PER_SUBSET_CONSTANTS*)MappedResource.pData;
        pData->m_iPatchStartIndex = (int)pSubset->IndexStart;
        pd3dDeviceContext->Unmap( m_pPerSubsetCB, 0 );
        pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerSubset, 1, &m_pPerSubsetCB );

        // Set up the material for this subset
        SetupMaterial( pd3dDeviceContext, pSubset->MaterialID );

        // Draw
        UINT NumIndices = (UINT)pSubset->IndexCount * MAX_EXTRAORDINARY_POINTS;
        UINT StartIndex = (UINT)pSubset->IndexStart * MAX_EXTRAORDINARY_POINTS;
        pd3dDeviceContext->DrawIndexed( NumIndices, StartIndex, 0 );
    }
}

//--------------------------------------------------------------------------------------
// Sets the specified material parameters (textures) into the D3D device.
//--------------------------------------------------------------------------------------
void CSubDMesh::SetupMaterial( ID3D11DeviceContext* pd3dDeviceContext, int MaterialID )
{
    SDKMESH_MATERIAL* pMaterial = m_pMeshFile->GetMaterial( MaterialID );

    ID3D11ShaderResourceView* Resources[] = { pMaterial->pNormalRV11, pMaterial->pDiffuseRV11, pMaterial->pSpecularRV11 };
    
    // The domain shader only needs the heightmap, so we only set 1 slot here.
    pd3dDeviceContext->DSSetShaderResources( 0, 1, Resources );

    // The pixel shader samples from all 3 textures.
    pd3dDeviceContext->PSSetShaderResources( 0, 3, Resources );
}


//--------------------------------------------------------------------------------------
void CSubDMesh::Destroy()
{
    int PieceCount = GetNumMeshPieces();
    for( int i = 0; i < PieceCount; ++i )
    {
        MeshPiece* pPiece = m_MeshPieces[i];

        SAFE_RELEASE( pPiece->m_pControlPointVB );
        SAFE_RELEASE( pPiece->m_pPerPatchDataSRV );
        SAFE_RELEASE( pPiece->m_pPerPatchDataVB );
        SAFE_RELEASE( pPiece->m_pExtraordinaryPatchIB );

        delete pPiece;
    }
    m_MeshPieces.RemoveAll();

    SAFE_RELEASE( m_pPerSubsetCB );

    if( m_pMeshFile != NULL )
    {
        int MaterialCount = m_pMeshFile->GetNumMaterials();
        for( int i = 0; i < MaterialCount; ++i )
        {
            SDKMESH_MATERIAL* pMaterial = m_pMeshFile->GetMaterial( i );

            if( pMaterial->pDiffuseRV11 == g_pDefaultDiffuseSRV )
            {
                pMaterial->pDiffuseRV11 = NULL;
            }

            if( pMaterial->pNormalRV11 == g_pDefaultNormalSRV )
            {
                pMaterial->pNormalRV11 = NULL;
            }

            if( pMaterial->pSpecularRV11 == g_pDefaultSpecularSRV )
            {
                pMaterial->pSpecularRV11 = NULL;
            }
        }
    }

    SAFE_RELEASE( g_pDefaultDiffuseSRV );
    SAFE_RELEASE( g_pDefaultDiffuseTexture );
    SAFE_RELEASE( g_pDefaultNormalSRV );
    SAFE_RELEASE( g_pDefaultNormalTexture );
    SAFE_RELEASE( g_pDefaultSpecularSRV );
    SAFE_RELEASE( g_pDefaultSpecularTexture );

    if( m_pMeshFile != NULL )
    {
        m_pMeshFile->Destroy();
    }
}
