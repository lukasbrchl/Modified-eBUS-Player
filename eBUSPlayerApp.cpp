// *****************************************************************************
//
//     Copyright (c) 2007, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "stdafx.h"
#include "eBUSPlayerApp.h"
#include "eBUSPlayerDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


BEGIN_MESSAGE_MAP( eBUSPlayerApp, CWinApp )
END_MESSAGE_MAP()


///
/// \brief Constructor
///

eBUSPlayerApp::eBUSPlayerApp()
{
}


//
// The one and only eBUSPlayerApp object
//

eBUSPlayerApp theApp;



///
/// \brief App initialization
///

BOOL eBUSPlayerApp::InitInstance()
{
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);

    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CCommandLineInfo lCommandLine;
    ParseCommandLine( lCommandLine );
    CWinApp::InitInstance();

    AfxEnableControlContainer();

    SetRegistryKey( _T( "Pleora Technologies Inc" ) );

    eBUSPlayerDlg dlg( lCommandLine.m_strFileName );
    m_pMainWnd = &dlg;

    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with OK
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with Cancel
    }

    // Since the dialog has been closed, return FALSE so that we exit the
    //  application, rather than start the application's message pump.
    return FALSE;
}


