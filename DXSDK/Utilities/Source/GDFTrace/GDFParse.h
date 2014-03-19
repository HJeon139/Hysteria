//--------------------------------------------------------------------------------------
// File: GDFParse.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "d3dx10.h"

class CGDFParse
{
public:
    CGDFParse();
    ~CGDFParse();

    HRESULT LoadXML( WCHAR* strGDFPath );

    HRESULT ExtractXML( WCHAR* strGDFBinPath, WORD wLanguage = LANG_NEUTRAL );

    HRESULT ValidateGDF( WCHAR* strGDFPath, WCHAR* strReason, int cchReason );

    HRESULT ValidateXML( WCHAR* strGDFBinPath, WORD wLanguage, WCHAR* strReason, int cchReason );

    HRESULT EnumLangs( WCHAR* strGDFBinPath );
    int GetNumLangs() const { return m_LanguageCount; }
    WORD GetLang( int iIndex ) const { return m_Languages[iIndex]; } 

    // To use these, call ExtractXML() first
    HRESULT GetName( WCHAR* strDest, int cchDest );
    HRESULT GetDescription( WCHAR* strDest, int cchDest );
    HRESULT GetReleaseDate( WCHAR* strDest, int cchDest );
    HRESULT GetGenre( WCHAR* strDest, int cchDest );
    HRESULT GetDeveloper( WCHAR* strDest, int cchDest );
    HRESULT GetDeveloperLink( WCHAR* strDest, int cchDest );
    HRESULT GetPublisher( WCHAR* strDest, int cchDest );
    HRESULT GetPublisherLink( WCHAR* strDest, int cchDest );
    HRESULT GetVersion( WCHAR* strDest, int cchDest );
    HRESULT GetWinSPR( float* pnMin, float* pnRecommended );
    HRESULT GetGameID( WCHAR* strDest, int cchDest );
    HRESULT GetRatingSystem( int iRating, WCHAR* strRatingSystem, int cchRatingSystem );
    HRESULT GetRatingID( int iRating, WCHAR* strRatingID, int cchRatingID );
    HRESULT GetRatingDescriptor( int iRating, int iDescriptor, WCHAR* strDescriptor, int cchDescriptor );
    HRESULT GetGameExe( int iGameExe, WCHAR* strEXE, int cchEXE );
    HRESULT GetSavedGameFolder( WCHAR* strDest, int cchDest );
    HRESULT GetType( WCHAR* strDest, int cchDest );
    HRESULT GetRSS( WCHAR* strDest, int cchDest );

    HRESULT ExtractGDFThumbnail( WCHAR* strGDFBinPath, D3DX10_IMAGE_INFO* info, WORD wLanguage = LANG_NEUTRAL );

    BOOL IsIconPresent( WCHAR* strGDFBinPath );

protected:
    HRESULT LoadXMLinMemory( WCHAR* strGDFBinPath, WORD wLanguage, HGLOBAL* phResourceCopy );
    bool ValidateUsingSchema( VOID* pDoc, WCHAR* strReason, int cchReason );
    HRESULT GetXMLValue( WCHAR* strXPath, WCHAR* strValue, int cchValue );
    HRESULT GetXMLAttrib( WCHAR* strXPath, WCHAR* strAttribName, WCHAR* strValue, int cchValue );
    HRESULT GetAttribFromNode( IXMLDOMNode* pNode, WCHAR* strAttrib, WCHAR* strDest, int cchDest );
    HRESULT ConvertGuidToFolderName( WCHAR* strFolderGuid, WCHAR* strFolderName );
    static BOOL CALLBACK StaticEnumResLangProc( HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage, LONG_PTR lParam );
    BOOL EnumResLangProc( HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage );

    IXMLDOMNode* m_pRootNode;
    bool m_bCleanupCOM;

    WORD m_Languages[256];
    int m_LanguageCount;
};