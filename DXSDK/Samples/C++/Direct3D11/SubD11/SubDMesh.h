//--------------------------------------------------------------------------------------
// File: SubDMesh.h
//
// This class encapsulates the mesh loading and housekeeping functions for a SubDMesh.
// The mesh loads preprocessed SDKMESH files from disk and stages them for rendering.
//
// To view the mesh preprocessing code, please find the ExportSubDMesh.cpp file in the 
// samples content exporter.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmesh.h"

// Maximum number of points that can be part of a subd quad.
// This includes the 4 interior points of the quad, plus the 1-ring neighborhood.
#define MAX_EXTRAORDINARY_POINTS 32

// Maximum valence we expect to encounter for extraordinary vertices
#define MAX_VALENCE 16

// Control point for a sub-d patch
struct SUBD_CONTROL_POINT
{
	D3DXVECTOR3 m_vPosition;
    BYTE		m_Weights[4];
    BYTE		m_Bones[4];
    D3DXVECTOR3 m_vNormal;      // Normal is not used for patch computation.
	D3DXVECTOR2 m_vUV;
	D3DXVECTOR3 m_vTanU;
};

//--------------------------------------------------------------------------------------
// This class handles most of the loading and conversion for a subd mesh.  It also
// creates and tracks buffers used by the mesh.
//--------------------------------------------------------------------------------------
class CSubDMesh
{
private:
    struct MeshPiece
    {
        SDKMESH_MESH*   m_pMesh;
        UINT            m_MeshIndex;

        ID3D11Buffer*   m_pExtraordinaryPatchIB;        // Index buffer for patches
        ID3D11Buffer*   m_pControlPointVB;              // Stores the control points for mesh
        ID3D11Buffer*   m_pPerPatchDataVB;              // Stores valences and prefixes on a per-patch basis
        ID3D11ShaderResourceView* m_pPerPatchDataSRV;   // Per-patch SRV
        INT             m_iPatchCount;

        D3DXVECTOR3     m_vCenter;
        D3DXVECTOR3     m_vExtents;
        INT             m_iFrameIndex;
    };

    CGrowableArray<MeshPiece*>  m_MeshPieces;

    CDXUTSDKMesh*   m_pMeshFile;

    ID3D11Buffer*   m_pPerSubsetCB;

public:
                ~CSubDMesh();

    // Loading
    HRESULT     LoadSubDFromSDKMesh( ID3D11Device* pd3dDevice, const WCHAR* strFileName, const WCHAR* strAnimationFileName );
    void        Destroy();

    void        RenderMeshPiece( ID3D11DeviceContext* pd3dDeviceContext, int PieceIndex );

    // Per-frame update
    void        Update( D3DXMATRIX* pWorld, double fTime );

    // Accessors
    int         GetNumInfluences( UINT iMeshPiece )
    {
        return m_pMeshFile->GetNumInfluences( iMeshPiece );
    }
    const D3DXMATRIX* GetInfluenceMatrix( UINT iMeshPiece, UINT iInfluence )
    {
        return m_pMeshFile->GetMeshInfluenceMatrix( iMeshPiece, iInfluence );
    }
    int         GetNumMeshPieces()
    {
        return m_MeshPieces.GetSize();
    }
    bool        GetMeshPieceTransform( UINT iMeshPiece, D3DXMATRIX* pTransform )
    {
        INT iFrameIndex = m_MeshPieces[iMeshPiece]->m_iFrameIndex;
        if( iFrameIndex == -1 )
        {
            D3DXMatrixIdentity( pTransform );
        }
        else
        {
            *pTransform = *( m_pMeshFile->GetWorldMatrix( iFrameIndex ) );
        }
        return true;
    }

protected:
    void SetupMaterial( ID3D11DeviceContext* pd3dDeviceContext, int MaterialID );
};
