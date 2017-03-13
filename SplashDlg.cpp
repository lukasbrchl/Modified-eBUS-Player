// *****************************************************************************
//
//     Copyright (c) 2010, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "stdafx.h"

#include "SplashDlg.h"

#include <PvVersion.h>


#ifdef _PT_DEBUG_
#pragma warning ( push )
#pragma warning ( disable : 4100 )
#endif // _PT_DEBUG_


BEGIN_MESSAGE_MAP( SplashDlg, CDialog )
    ON_WM_TIMER()
END_MESSAGE_MAP()


// ==========================================================================
void SplashDlg::Show()
{
    SplashDlg lDlg;
    lDlg.DoModal();
}

#ifdef _PT_DEBUG_
#pragma warning( pop )
#endif // _PT_DEBUG_

// ==========================================================================
SplashDlg::SplashDlg( CWnd* pParent /*=NULL*/ ) 
    : CDialog( SplashDlg::IDD, pParent )
{
}

// ==========================================================================
SplashDlg::~SplashDlg()
{
}

// =============================================================================
void SplashDlg::DoDataExchange( CDataExchange* pDX )
{
    CDialog::DoDataExchange( pDX );
    DDX_Control(pDX, IDC_APPNAME, mAppNameLabel);
    DDX_Control(pDX, IDC_PRODUCTNAME, mProductNameLabel);
    DDX_Control(pDX, IDC_COPYRIGHT, mCopyrightLabel);
    DDX_Control(pDX, IDC_COMPANY, mCompanyLabel);
}

// ==========================================================================
void SplashDlg::OnTimer( UINT_PTR aEvent )
{
    KillTimer( aEvent );
    ShowWindow( SW_HIDE );

    EndDialog( IDOK );
}

// ==========================================================================
BOOL SplashDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetTimer( 0, 2500, NULL );

    // Create bold font
    LOGFONT lLogFont;
    mAppNameLabel.GetFont()->GetLogFont( &lLogFont );
    lLogFont.lfWeight = FW_BOLD;
    mBoldFont.CreateFontIndirect( &lLogFont );

    mAppNameLabel.SetFont( &mBoldFont );

    mAppNameLabel.SetWindowText( _T( "eBUS Player" ) );
    mProductNameLabel.SetWindowText( CString( _T( "eBUS SDK Version " ) ) + CString( NVERSION_STRING ) );
    mCopyrightLabel.SetWindowText( _T( VERSION_COPYRIGHT ) );
    mCompanyLabel.SetWindowText( _T( VERSION_COMPANY_NAME ) );

    return TRUE;
}
