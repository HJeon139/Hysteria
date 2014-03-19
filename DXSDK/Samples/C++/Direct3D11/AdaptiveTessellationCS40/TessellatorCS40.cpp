//--------------------------------------------------------------------------------------
// File: TessellatorCS40.cpp
//
// Demos how to use Compute Shader 4.0 to do one simple adaptive tessellation scheme
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "ScanCS.h"
#include "TessellatorCS40.h"

ID3D11ComputeShader* CTessellator::s_pEdgeFactorCS = NULL;
ID3D11ComputeShader* CTessellator::s_pNumVerticesIndicesCS = NULL;
ID3D11ComputeShader* CTessellator::s_pScatterVertexTriIDIndexIDCS = NULL;
ID3D11ComputeShader* CTessellator::s_pScatterIndexTriIDIndexIDCS = NULL;
ID3D11ComputeShader* CTessellator::s_pTessVerticesCS = NULL;
ID3D11ComputeShader* CTessellator::s_pTessIndicesCS = NULL;
ID3D11Buffer*        CTessellator::s_pEdgeFactorCSCB = NULL;
ID3D11Buffer*        CTessellator::s_pCSCB = NULL;
ID3D11Buffer*        CTessellator::s_pCSReadBackBuf = NULL;

CScanCS              CTessellator::s_ScanCS;

HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut );

CTessellator::CTessellator()
    : m_pd3dDevice(NULL),
        m_pd3dImmediateContext(NULL),
        m_pBaseVB(NULL),
        m_pBaseVBSRV(NULL),
        m_pEdgeFactorBufSRV(NULL),
        m_pEdgeFactorBufUAV(NULL),
        m_pScanBuf0(NULL),
        m_pScanBuf0SRV(NULL),
        m_pScanBuf0UAV(NULL),
        m_pScatterVertexBuf(NULL),
        m_pScatterIndexBuf(NULL),
        m_pScatterVertexBufSRV(NULL),
        m_pScatterIndexBufSRV(NULL),
        m_pScatterVertexBufUAV(NULL),
        m_pScatterIndexBufUAV(NULL),
        m_pTessedVerticesBufSRV(NULL),
        m_pTessedVerticesBufUAV(NULL),
        m_pTessedIndicesBufUAV(NULL),
        m_nCachedTessedVertices(0),
        m_nCachedTessedIndices(0),
        m_nVertices(0)
{        
}

CTessellator::~CTessellator()
{
    DeleteDeviceObjects();    
}

void CTessellator::DeleteDeviceObjects()
{
    SAFE_RELEASE( m_pEdgeFactorBufSRV );
    SAFE_RELEASE( m_pEdgeFactorBufUAV );
    SAFE_RELEASE( m_pEdgeFactorBuf );
    SAFE_RELEASE( m_pScanBuf0 );
    SAFE_RELEASE( m_pScanBuf1 );
    SAFE_RELEASE( m_pScanBuf0SRV );
    SAFE_RELEASE( m_pScanBuf1SRV );
    SAFE_RELEASE( m_pScanBuf0UAV );
    SAFE_RELEASE( m_pScanBuf1UAV );
    SAFE_RELEASE( m_pScatterIndexBuf );
    SAFE_RELEASE( m_pScatterVertexBuf );
    SAFE_RELEASE( m_pScatterVertexBufSRV );
    SAFE_RELEASE( m_pScatterIndexBufSRV );
    SAFE_RELEASE( m_pScatterVertexBufUAV );
    SAFE_RELEASE( m_pScatterIndexBufUAV );
    SAFE_RELEASE( m_pTessedVerticesBufSRV );
    SAFE_RELEASE( m_pTessedVerticesBufUAV );
    SAFE_RELEASE( m_pTessedIndicesBufUAV );
    SAFE_RELEASE( m_pBaseVBSRV );
}

HRESULT CTessellator::OnD3D11CreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;
    V_RETURN( s_ScanCS.OnD3D11CreateDevice( pd3dDevice ) );

    return hr;
}

void CTessellator::OnDestroyDevice()
{
    s_ScanCS.OnD3D11DestroyDevice();
    
    DeleteDeviceObjects();
    
    SAFE_RELEASE( s_pCSReadBackBuf );
    SAFE_RELEASE( s_pEdgeFactorCSCB );
    SAFE_RELEASE( s_pCSCB );
    SAFE_RELEASE( s_pEdgeFactorCS );
    SAFE_RELEASE( s_pNumVerticesIndicesCS );
    SAFE_RELEASE( s_pScatterVertexTriIDIndexIDCS );
    SAFE_RELEASE( s_pScatterIndexTriIDIndexIDCS );
    SAFE_RELEASE( s_pTessVerticesCS );
    SAFE_RELEASE( s_pTessIndicesCS );
}

HRESULT CTessellator::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    const float adaptive_scale_in_pixels= 30.0f;
    m_tess_edge_len_scale.x= (pBackBufferSurfaceDesc->Width * 0.5f) / adaptive_scale_in_pixels;
    m_tess_edge_len_scale.y= (pBackBufferSurfaceDesc->Height * 0.5f) / adaptive_scale_in_pixels;

    return S_OK;
}

HRESULT CTessellator::SetBaseMesh( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                   INT nVertices,
                                   ID3D11Buffer* pBaseVB )
{
    DeleteDeviceObjects();

    HRESULT hr;
    
    m_pd3dDevice = pd3dDevice;
    m_pd3dImmediateContext = pd3dImmediateContext;
    m_nVertices = nVertices;

    if ( s_pEdgeFactorCS == NULL )
    {
        ID3DBlob* pBlobCS = NULL;
        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_EdgeFactorCS.hlsl", "CSEdgeFactor", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pEdgeFactorCS ) );
        SAFE_RELEASE( pBlobCS );

        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_NumVerticesIndicesCS.hlsl", "CSNumVerticesIndices", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pNumVerticesIndicesCS ) );
        SAFE_RELEASE( pBlobCS );

        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_ScatterIDCS.hlsl", "CSScatterVertexTriIDIndexID", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pScatterVertexTriIDIndexIDCS ) );
        SAFE_RELEASE( pBlobCS );

        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_ScatterIDCS.hlsl", "CSScatterIndexTriIDIndexID", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pScatterIndexTriIDIndexIDCS ) );
        SAFE_RELEASE( pBlobCS );

        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_TessellateVerticesCS.hlsl", "CSTessellationVertices", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pTessVerticesCS ) );
        SAFE_RELEASE( pBlobCS );

        V_RETURN( CompileShaderFromFile( L"TessellatorCS40_TessellateIndicesCS.hlsl", "CSTessellationIndices", "cs_4_0", &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &s_pTessIndicesCS ) );
        SAFE_RELEASE( pBlobCS );

        // constant buffers used to pass parameters to CS
        D3D11_BUFFER_DESC Desc;
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags = 0;    
        Desc.ByteWidth = sizeof( CB_EdgeFactorCS );
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &s_pEdgeFactorCSCB ) );

        Desc.ByteWidth = sizeof(INT) * 4;
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &s_pCSCB ) );

        // read back buffer
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Desc.Usage = D3D11_USAGE_STAGING;
        Desc.BindFlags = 0;
        Desc.MiscFlags = 0;
        Desc.ByteWidth = sizeof(INT) * 2;
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &s_pCSReadBackBuf ) );
    }

    // shader resource view of base mesh vertex data 
    D3D11_SHADER_RESOURCE_VIEW_DESC DescRV;
    ZeroMemory( &DescRV, sizeof( DescRV ) );
    DescRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    DescRV.Buffer.FirstElement = 0;
    DescRV.Buffer.NumElements = nVertices;
    V_RETURN( pd3dDevice->CreateShaderResourceView( pBaseVB, &DescRV, &m_pBaseVBSRV ) );

    // Buffer for edge tessellation factor
    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.ByteWidth = sizeof(float) * m_nVertices;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(float);
    desc.Usage = D3D11_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateBuffer(&desc, NULL, &m_pEdgeFactorBuf) );    

    // shader resource view of the buffer above
    ZeroMemory( &DescRV, sizeof( DescRV ) );
    DescRV.Format = DXGI_FORMAT_UNKNOWN;
    DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    DescRV.Buffer.FirstElement = 0;
    DescRV.Buffer.NumElements = m_nVertices;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pEdgeFactorBuf, &DescRV, &m_pEdgeFactorBufSRV ) );

    // UAV of the buffer above
    D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
    ZeroMemory( &DescUAV, sizeof(DescUAV) );
    DescUAV.Format = DXGI_FORMAT_UNKNOWN;
    DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    DescUAV.Buffer.FirstElement = 0;
    DescUAV.Buffer.NumElements = m_nVertices;
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pEdgeFactorBuf, &DescUAV, &m_pEdgeFactorBufUAV ) );

    // Buffers for scan
    desc.ByteWidth = sizeof(INT)* 2 * m_nVertices / 3;
    desc.StructureByteStride = sizeof(INT)* 2;
    V_RETURN( pd3dDevice->CreateBuffer(&desc, NULL, &m_pScanBuf0) );
    V_RETURN( pd3dDevice->CreateBuffer(&desc, NULL, &m_pScanBuf1) );

    // shader resource views of the scan buffers
    DescRV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pScanBuf0, &DescRV, &m_pScanBuf0SRV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pScanBuf1, &DescRV, &m_pScanBuf1SRV ) );

    // UAV of the scan buffers
    DescUAV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pScanBuf0, &DescUAV, &m_pScanBuf0UAV ) );
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pScanBuf1, &DescUAV, &m_pScanBuf1UAV ) );

    return S_OK;
}

ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    pDevice->CreateBuffer(&desc, NULL, &debugbuf);

    pd3dImmediateContext->CopyResource( debugbuf, pBuffer );

    return debugbuf;
}

void RunComputeShader( ID3D11DeviceContext* pd3dImmediateContext,
                       ID3D11ComputeShader* pComputeShader,
                       UINT nNumViews, ID3D11ShaderResourceView** pShaderResourceViews, 
                       ID3D11Buffer* pCBCS, void* pCSData, DWORD dwNumDataBytes,
                       ID3D11UnorderedAccessView* pUnorderedAccessView,
                       UINT X, UINT Y, UINT Z )
{
    HRESULT hr = S_OK;
    
    pd3dImmediateContext->CSSetShader( pComputeShader, NULL, 0 );
    pd3dImmediateContext->CSSetShaderResources( 0, nNumViews, pShaderResourceViews );
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pUnorderedAccessView, (UINT*)&pUnorderedAccessView );
    if ( pCBCS )
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        V( pd3dImmediateContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        memcpy( MappedResource.pData, pCSData, dwNumDataBytes );
        pd3dImmediateContext->Unmap( pCBCS, 0 );
        ID3D11Buffer* ppCB[1] = { pCBCS };
        pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppCB );
    }

    pd3dImmediateContext->Dispatch( X, Y, Z );

    ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, (UINT*)(&ppUAViewNULL) );

    ID3D11ShaderResourceView* ppSRVNULL[3] = { NULL, NULL, NULL };
    pd3dImmediateContext->CSSetShaderResources( 0, 3, ppSRVNULL );
    pd3dImmediateContext->CSSetConstantBuffers( 0, 0, NULL );
}

void CTessellator::PerEdgeTessellation( D3DXMATRIX* matWVP,
                                        ID3D11Buffer** ppTessedVerticesBuf, ID3D11Buffer** ppTessedIndicesBuf, 
                                        DWORD* num_tessed_vertices, DWORD* num_tessed_indices )
{
    HRESULT hr;
    
    // Update per-edge tessellation factors
    {
        CB_EdgeFactorCS cbCS;
        cbCS.matWVP = *matWVP;
        cbCS.tess_edge_length_scale = m_tess_edge_len_scale;

        ID3D11ShaderResourceView* aRViews[1] = { m_pBaseVBSRV };
        RunComputeShader( m_pd3dImmediateContext, 
                          s_pEdgeFactorCS,
                          1, aRViews,
                          s_pEdgeFactorCSCB, &cbCS, sizeof(cbCS),
                          m_pEdgeFactorBufUAV,
                          m_nVertices/3, 1, 1 );
    }    

    // How many vertices/indices are needed for the tessellated mesh?
    {
        ID3D11ShaderResourceView* aRViews[1] = { m_pEdgeFactorBufSRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pNumVerticesIndicesCS,
                          1, aRViews,
                          NULL, NULL, 0,
                          m_pScanBuf0UAV,
                          m_nVertices/3, 1, 1 );
        s_ScanCS.ScanCS( m_pd3dImmediateContext, m_nVertices/3, m_pScanBuf0SRV, m_pScanBuf0UAV, m_pScanBuf1SRV, m_pScanBuf1UAV );

        // read back the number of vertices/indices for tessellation output
        D3D11_BOX box;
        box.left = sizeof(INT)*2 * m_nVertices/3 - sizeof(INT)*2;
        box.right = sizeof(INT)*2 * m_nVertices/3;
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        m_pd3dImmediateContext->CopySubresourceRegion(s_pCSReadBackBuf, 0, 0, 0, 0, m_pScanBuf0, 0, &box);
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        V( m_pd3dImmediateContext->Map( s_pCSReadBackBuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );       
        *num_tessed_vertices = ((DWORD*)MappedResource.pData)[0];
        *num_tessed_indices = ((DWORD*)MappedResource.pData)[1];
        m_pd3dImmediateContext->Unmap( s_pCSReadBackBuf, 0 );
    }

    if ( *num_tessed_vertices == 0 || *num_tessed_indices == 0 )
        return;

#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, m_pScanBuf0 );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT v, t;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif


    // Generate buffers for scattering TriID and IndexID for both vertex data and index data,
    // also generate buffers for output tessellated vertex data and index data
    {
        if ( m_pScatterVertexBuf == NULL || m_nCachedTessedVertices < *num_tessed_vertices )
        {
            SAFE_RELEASE( m_pScatterVertexBuf );
            SAFE_RELEASE( m_pScatterVertexBufSRV );
            SAFE_RELEASE( m_pScatterVertexBufUAV );

            SAFE_RELEASE( *ppTessedVerticesBuf );
            SAFE_RELEASE( m_pTessedVerticesBufUAV );
            SAFE_RELEASE( m_pTessedVerticesBufSRV );
            
            D3D11_BUFFER_DESC desc;
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(INT) * 2 * *num_tessed_vertices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(INT) * 2;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, NULL, &m_pScatterVertexBuf) );  

            D3D11_SHADER_RESOURCE_VIEW_DESC DescRV;
            ZeroMemory( &DescRV, sizeof( DescRV ) );
            DescRV.Format = DXGI_FORMAT_UNKNOWN;
            DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            DescRV.Buffer.FirstElement = 0;
            DescRV.Buffer.NumElements = *num_tessed_vertices;
            V( m_pd3dDevice->CreateShaderResourceView( m_pScatterVertexBuf, &DescRV, &m_pScatterVertexBufSRV ) );

            D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
            ZeroMemory( &DescUAV, sizeof(DescUAV) );
            DescUAV.Format = DXGI_FORMAT_UNKNOWN;
            DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            DescUAV.Buffer.FirstElement = 0;
            DescUAV.Buffer.NumElements = *num_tessed_vertices;
            V( m_pd3dDevice->CreateUnorderedAccessView( m_pScatterVertexBuf, &DescUAV, &m_pScatterVertexBufUAV ) );

            // generate the output tessellated vertices buffer
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(float) * 3 * *num_tessed_vertices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(float) * 3;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, NULL, ppTessedVerticesBuf) );
            V( m_pd3dDevice->CreateUnorderedAccessView( *ppTessedVerticesBuf, &DescUAV, &m_pTessedVerticesBufUAV ) );
            V( m_pd3dDevice->CreateShaderResourceView( *ppTessedVerticesBuf, &DescRV, &m_pTessedVerticesBufSRV ) );

            m_nCachedTessedVertices = *num_tessed_vertices;
        }

        if ( m_pScatterIndexBuf == NULL || m_nCachedTessedIndices < *num_tessed_indices )
        {
            SAFE_RELEASE( m_pScatterIndexBuf );
            SAFE_RELEASE( m_pScatterIndexBufSRV );
            SAFE_RELEASE( m_pScatterIndexBufUAV );

            SAFE_RELEASE( *ppTessedIndicesBuf );
            SAFE_RELEASE( m_pTessedIndicesBufUAV );
            
            D3D11_BUFFER_DESC desc;
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(INT) * 2 * *num_tessed_indices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(INT) * 2;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, NULL, &m_pScatterIndexBuf) );  

            D3D11_SHADER_RESOURCE_VIEW_DESC DescRV;
            ZeroMemory( &DescRV, sizeof( DescRV ) );
            DescRV.Format = DXGI_FORMAT_UNKNOWN;
            DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            DescRV.Buffer.FirstElement = 0;
            DescRV.Buffer.NumElements = *num_tessed_indices;
            V( m_pd3dDevice->CreateShaderResourceView( m_pScatterIndexBuf, &DescRV, &m_pScatterIndexBufSRV ) );

            D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
            ZeroMemory( &DescUAV, sizeof(DescUAV) );
            DescUAV.Format = DXGI_FORMAT_UNKNOWN;
            DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            DescUAV.Buffer.FirstElement = 0;
            DescUAV.Buffer.NumElements = *num_tessed_indices;
            V( m_pd3dDevice->CreateUnorderedAccessView( m_pScatterIndexBuf, &DescUAV, &m_pScatterIndexBufUAV ) );

            // generate the output tessellated indices buffer
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_INDEX_BUFFER;
            desc.ByteWidth = sizeof(UINT) * *num_tessed_indices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, NULL, ppTessedIndicesBuf) );

            DescUAV.Format = DXGI_FORMAT_R32_TYPELESS;
            DescUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
            V( m_pd3dDevice->CreateUnorderedAccessView( *ppTessedIndicesBuf, &DescUAV, &m_pTessedIndicesBufUAV ) );

            m_nCachedTessedIndices = *num_tessed_indices;
        }
    }

    // Scatter TriID, IndexID
    {
        // Scatter vertex TriID, IndexID
        ID3D11ShaderResourceView* aRViews[1] = { m_pScanBuf0SRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pScatterVertexTriIDIndexIDCS,
                          1, aRViews,
                          NULL, NULL, 0,
                          m_pScatterVertexBufUAV,
                          m_nVertices/3, 1, 1 );
        
        // Scatter index TriID, IndexID
        RunComputeShader( m_pd3dImmediateContext,
                          s_pScatterIndexTriIDIndexIDCS,
                          1, aRViews,
                          NULL, NULL, 0,
                          m_pScatterIndexBufUAV,
                          m_nVertices/3, 1, 1 );
    }

#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext , m_pScatterVertexBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT v, t;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );        
    }
#endif

    // Tessellate vertex
    {
        INT cbCS[4] = {*num_tessed_vertices, 0, 0, 0};
        ID3D11ShaderResourceView* aRViews[2] = { m_pScatterVertexBufSRV, m_pEdgeFactorBufSRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pTessVerticesCS,
                          2, aRViews,
                          s_pCSCB, cbCS, sizeof(INT)*4,
                          m_pTessedVerticesBufUAV,
                          INT(ceil(*num_tessed_vertices/128.0f)), 1, 1 );
    }

#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, *ppTessedVerticesBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT id;
            float u; float v;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif

    // Tessellate indices
    {
        INT cbCS[4] = {*num_tessed_indices, 0, 0, 0};
        ID3D11ShaderResourceView* aRViews[3] = { m_pScatterIndexBufSRV, m_pEdgeFactorBufSRV, m_pScanBuf0SRV };
        RunComputeShader( m_pd3dImmediateContext,
            s_pTessIndicesCS,
            3, aRViews,
            s_pCSCB, cbCS, sizeof(INT)*4,
            m_pTessedIndicesBufUAV,
            INT(ceil(*num_tessed_indices/128.0f)), 1, 1 );
    }

#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, *ppTessedIndicesBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        INT *p = NULL;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (INT *)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif
}