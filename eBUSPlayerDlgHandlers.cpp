// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "stdafx.h"
#include "eBUSPlayerDlg.h"

#include "SplashDlg.h"
#include "PvMessageBox.h"
#include "SetupDlg.h"
#include "WarningDlg.h"

#include <PvDeviceFinderWnd.h>
#include <PvNetworkAdapter.h>


// =============================================================================
BOOL eBUSPlayerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SplashDlg::Show();

    mFilteringDlg->Create( IDD_FILTERING, this );

    mYellowColor = RGB( 0xFF, 0xFF, 0x00 );
    mYellowBrush.CreateSolidBrush( mYellowColor );

    mRedColor = RGB( 0x80, 0x00, 0x00 );
    mRedBrush.CreateSolidBrush( mRedColor );

    SetStatusColor( SCDefault );

    GetClientRect( mCrt );
    mNeedInit = FALSE;

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    CRect lDisplayRect;
    GetDlgItem( IDC_DISPLAYPOS )->GetClientRect( &lDisplayRect );
    GetDlgItem( IDC_DISPLAYPOS )->ClientToScreen( &lDisplayRect );
    ScreenToClient( &lDisplayRect );
    mDisplay.Create( GetSafeHwnd(), 10000 );
    mDisplay.SetPosition( lDisplayRect.left, lDisplayRect.top, lDisplayRect.Width(), lDisplayRect.Height() );
    mDisplay.SetBackgroundColor( 0x80, 0x80, 0x80 );

    EnableInterface();

    LoadMRUFromRegistry();
    UpdateMRUMenu();

    // create a defaults persistence file if it doesn't already exist
    CString lDefaultPath = GetDefaultPath();
    SaveConfig( lDefaultPath, false );

    if ( !mFileName.IsEmpty() )
    {
        OpenConfig( mFileName );
    }
    else
    {
        // check for the existence of the sticky configuration file. If it exists, load it.
        CString lStickyPath = GetStickyPath();
        if ( _taccess( lStickyPath.GetBuffer(), 0 ) == 0 )
        {
            OpenConfig( lStickyPath );
        }
    }

    mAccel = ::LoadAccelerators(AfxGetResourceHandle(),
        MAKEINTRESOURCE(IDR_ACCELERATOR));

    return TRUE;  // return TRUE  unless you set the focus to a control
}


// =============================================================================
void eBUSPlayerDlg::OnPaint()
{
    if ( IsIconic() )
    {
        CPaintDC dc( this ); // device context for painting

        SendMessage (WM_ICONERASEBKGND, reinterpret_cast<WPARAM>( dc.GetSafeHdc() ), 0 );

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics( SM_CXICON );
        int cyIcon = GetSystemMetrics( SM_CYICON );
        CRect rect;
        GetClientRect( &rect );
        int x = ( rect.Width() - cxIcon + 1 ) / 2;
        int y = ( rect.Height() - cyIcon + 1 ) / 2;

        // Draw the icon
        dc.DrawIcon( x, y, m_hIcon );
    }
    else
    {
        CDialog::OnPaint();
    }
}


// =============================================================================
HCURSOR eBUSPlayerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>( m_hIcon );
}


// =============================================================================
void eBUSPlayerDlg::OnClose()
{
    // Make sure we cleanup before we leave
    Disconnect();

    // Close action command
    if ( mActionCommandDlg != NULL )
    {
        if ( mActionCommandDlg->GetSafeHwnd() != 0 )
        {
            mActionCommandDlg->DestroyWindow();
        }

        delete mActionCommandDlg;
        mActionCommandDlg = NULL;
    }

    DestroyWindow();
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedConnectButton()
{
    // create a device finder wnd and open the select device dialog
    PvDeviceFinderWnd lFinder;
    lFinder.SetTitle( _T( "Device Selection" ) );

    PvResult lResult = lFinder.ShowModal();
    if ( !lResult.IsOK() || ( lFinder.GetSelected() == NULL ) )
    {
        return;
    }

    const PvNetworkAdapter *lNetworkAdapter = dynamic_cast<const PvNetworkAdapter *>( lFinder.GetSelected()->GetInterface() );
    if ( lNetworkAdapter != NULL )
    {
        if ( !lNetworkAdapter->IsPleoraDriverInstalled() )
        {
            UINT lDontShowAgain = AfxGetApp()->GetProfileInt( _T(""), _T("DontShowNoDriverWarning"), 0 );

            if ( lDontShowAgain == 0 )
            {
                WarningDlg lNoDriverWarning;
                lNoDriverWarning.SetWarning( _T( "You have chosen to connect to a GigE Vision device through a network interface, which does not employ an eBUS driver.  eBUS drivers are recommended for optimal streaming performance." ) );
                lNoDriverWarning.SetCheckboxMessage( _T( "Don't show me this again" ) );
                lNoDriverWarning.DoModal();

                if ( lNoDriverWarning.IsChecked() )
                {
                    AfxGetApp()->WriteProfileInt(_T(""), _T("DontShowNoDriverWarning"), 1 );
                }
            }
        }
    }

    CWaitCursor lCursor;

    UpdateWindow();

    Connect( lFinder.GetSelected() );
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedDisconnectButton()
{
    CWaitCursor lCursor;

    Disconnect();
}


// =============================================================================
void eBUSPlayerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    CRect lR;
    this->GetWindowRect( &lR );
    // TRACE( "%i %i\n", lR.Width(), lR.Height() );

    if ( mNeedInit || cx == 0 || cy == 0 )
    {
        return;
    }

    int dx = cx - mCrt.Width();
    int dy = cy - mCrt.Height();

    GetClientRect(&mCrt);

    CRect r1;

    HDWP hDWP = ::BeginDeferWindowPos(20);

    //
    // Bottom left, just bring the group box down
    //

    int lBottomLeft[] =
    {
        IDC_CONTROL_GROUP
    };

    for ( int i = 0; i < sizeof( lBottomLeft ) / sizeof ( lBottomLeft[ 0 ] ); i++ )
    {
        CWnd *lChild = GetDlgItem( lBottomLeft[ i ] );
        if ( lChild != NULL )
        {
            lChild->GetWindowRect(&r1); ScreenToClient(&r1); 
            r1.bottom += dy;
            ::DeferWindowPos(
                hDWP, lChild->m_hWnd, NULL, 
                r1.left, r1.top, r1.Width(), r1.Height(),
                SWP_NOACTIVATE|SWP_NOZORDER);
        }
    }

    //
    // Right, make sure the display and group box fill the right part
    // of our window
    //

    int lDisplayGroup[] =
    {
        IDC_DISPLAY_GROUP, IDC_DISPLAYPOS
    };

    for ( int i = 0; i < sizeof( lDisplayGroup ) / sizeof ( lDisplayGroup[ 0 ] ); i++ )
    {
        CWnd *lChild = GetDlgItem( lDisplayGroup[ i ] );
        if ( lChild != NULL )
        {
            lChild->GetWindowRect(&r1); ScreenToClient(&r1); 
            r1.bottom += dy;
            r1.right += dx;
            ::DeferWindowPos(
                hDWP, lChild->m_hWnd, NULL, 
                r1.left, r1.top, r1.Width(), r1.Height(),
                SWP_NOACTIVATE|SWP_NOZORDER);

            if ( lDisplayGroup[i] == IDC_DISPLAYPOS )
            {
                mDisplay.SetPosition( 
                    r1.left, r1.top, 
                    r1.Width(), r1.Height() );
            }
        }
    }

    //
    // Last, the status box. Just like the display, but sticks to the bottom
    // and is not resized
    //

    CWnd *lBottomRight[] = 
    { 
        &mStatusTextBox
    };

    for ( int i = 0; i < sizeof( lBottomRight ) / sizeof ( lBottomRight[ 0 ] ); i++ )
    {
        CWnd *lChild = lBottomRight[ i ];
        if ( lChild != NULL )
        {
            lChild->GetWindowRect(&r1); ScreenToClient(&r1); 
            ::DeferWindowPos(
                hDWP, lChild->m_hWnd, NULL, 
                r1.left, r1.top + dy, r1.Width() + dx, r1.Height(),
                SWP_NOACTIVATE|SWP_NOZORDER);
        }
    }

    ::EndDeferWindowPos(hDWP);
}


// =============================================================================
void eBUSPlayerDlg::OnGetMinMaxInfo( MINMAXINFO *lpMMI )
{
    lpMMI->ptMinTrackSize.x = 800;
    lpMMI->ptMinTrackSize.y = 547;
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedDeviceButton()
{
    if ( !mPlayer->IsDeviceConnected() )
    {
        return;
    }

    if ( mDeviceWnd.GetHandle() != 0 )
    {
        // If already open, just toggle to closed...     
        CloseGenWindow( &mDeviceWnd );
        return;
    }

    CString lTabName = _T( "Device" );
    PvDeviceGEV *lDGEV = dynamic_cast<PvDeviceGEV *>( mPlayer->GetPvDevice() );
    PvDeviceU3V *lDU3V = dynamic_cast<PvDeviceU3V *>( mPlayer->GetPvDevice() );
    if ( lDGEV != NULL )
    {
        lTabName = _T( "GEV Device" );
    }
    if ( lDU3V != NULL )
    {
        lTabName = _T( "U3V Device" );
    }

    // Create, assign parameters, set title and show modeless
    mDeviceWnd.SetTitle( "Device Control" );
    mDeviceWnd.AddGenParameterArray( mPlayer->GetDeviceParameters(), (LPCTSTR)lTabName );

#ifdef SERIALBRIDGE
    PvSerialBridgeManagerWnd *lSBM = mPlayer->GetSerialBridgeManager();
    if ( lSBM != NULL )
    {
        for ( uint32_t i = 0; i < lSBM->GetPortCount(); i++ )
        {
            PvGenParameterArray *lArray = lSBM->GetGenParameterArray( i );
            if ( lArray != NULL )
            {
                PvString lName = lSBM->GetGenParameterArrayName( i );
                mDeviceWnd.AddGenParameterArray( lArray, lName );
            }
        }
    }
#endif // SERIALBRIDGE

    mDeviceWnd.ShowModeless( GetSafeHwnd() );
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedLinkButton()
{
    ShowGenWindow( 
        &mCommunicationWnd, 
        mPlayer->GetCommunicationParameters(), 
        _T( "Communication Control" ) );
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedStreamparamsButton()
{
    if ( !mPlayer->IsStreamOpened() )
    {
        return;
    }

    ShowGenWindow( 
        & mStreamParametersWnd, 
        mPlayer->GetStreamParameters(), 
        _T( "Image Stream Control" ) );
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedStart()
{
    if ( !mPlayer->IsDeviceConnected() )
    {
        return;
    }

    mPlayer->Play();
}


// =============================================================================
void eBUSPlayerDlg::OnBnClickedStop()
{
    if ( !mPlayer->IsDeviceConnected() )
    {
        return;
    }

    SetStatusColor( SCDefault );
    mStatusTextBox.SetWindowText( _T( "" ) );

    mPlayer->Stop();
}


// =============================================================================
void eBUSPlayerDlg::OnCbnSelchangeMode()
{
    if ( !mPlayer->IsDeviceConnected() )
    {
        return;
    }

    // No selection?
    if ( mModeCombo.GetCurSel() < 0 )
    {
        return;
    }

    // Retrieve item data value from combo box item
    uint64_t lValue = mModeCombo.GetItemData( mModeCombo.GetCurSel() );

    // Change acquisition mode
    PvResult lResult = mPlayer->SetAcquisitionMode( lValue );
    if ( !lResult.IsOK() )
    {
        PvMessageBox( this, lResult );
    }
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnImageDisplayed( WPARAM wParam, LPARAM lParam )
{
    // Stream opened, image save dlg exists, thread display is up
    if ( !mPlayer->IsStreamStarted() )
    {
        return 0;
    }

    PvString lText;
    bool lRecording = false;

    mPlayer->GetStatusText( lText, lRecording );

    if ( lRecording )
    {
        SetStatusColor( SCRed );
    }
    else
    {
        SetStatusColor( SCDefault );
    }

    mStatusTextBox.SetWindowText( lText.GetUnicode() );

    return 0;
}


// =============================================================================
HBRUSH eBUSPlayerDlg::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
    switch ( pWnd->GetDlgCtrlID() )
    {
    case IDC_STATUS:    
        switch ( mStatusColor )
        {
        case SCDefault:
            break;

        case SCYellow:
            pDC->SetBkColor( mYellowColor );
            return mYellowBrush;

        case SCRed:
            pDC->SetTextColor( RGB( 0xFF, 0xFF, 0xFF ) );
            pDC->SetBkColor( mRedColor );
            return mRedBrush;
        }
        break;
    }

    return CDialog::OnCtlColor( pDC, pWnd, nCtlColor );
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnLinkDisconnected( WPARAM wParam, LPARAM lParam )
{
    mPlayer->LinkDisconnected();

    if ( mPlayer->IsLinkRecoveryEnabled() )
    {
        SetStatusColor( SCYellow );
        mStatusTextBox.SetWindowText( _T( "Connection to device lost..." ) );

        CloseGenWindow( &mDeviceWnd );
        CloseGenWindow( &mCommunicationWnd );
        CloseGenWindow( &mStreamParametersWnd );

#ifdef SERIALBRIDGE
        PvSerialBridgeManagerWnd *lSBM = mPlayer->GetSerialBridgeManager();
        if ( ( lSBM != NULL ) && ( lSBM->GetHandle() != 0 ) )
        {
            lSBM->Close();
        }
#endif // SERIALBRIDGE

        EnableTreeBrowsers( FALSE );
        EnableControls( FALSE );
    }
    else
    {
        SetStatusColor( SCDefault );
        mStatusTextBox.SetWindowText( _T( "" ) );

        CWaitCursor lCursor;
        Disconnect();

        MessageBox( _T( "Connection to device lost." ), _T( "eBUS Player" ), MB_OK | MB_ICONINFORMATION );
    }

    return 0;
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnLinkReconnected( WPARAM wParam, LPARAM lParam )
{
    mPlayer->Recover();

    SetStatusColor( SCDefault );
    mStatusTextBox.SetWindowText( _T( "" ) );

    EnableInterface();

    return 0;
}


// =============================================================================
void eBUSPlayerDlg::OnToolsSetup()
{
    Setup *lSetup = mPlayer->GetSetup();
    lSetup->SetEnabled( !mPlayer->IsDeviceConnected() && !mPlayer->IsStreamOpened() );

    SetupDlg lDlg( lSetup, this );
    lDlg.DoModal();

    EnableInterface();
}


// =============================================================================
void eBUSPlayerDlg::OnToolsImagefiltering()
{
    if ( mFilteringDlg->IsWindowVisible() )
    {
        mFilteringDlg->ShowWindow( SW_HIDE );
    }
    else
    {
        mFilteringDlg->ShowWindow( SW_SHOW );
        mFilteringDlg->BringWindowToTop();
        mFilteringDlg->SetFocus();
    }
}


// =============================================================================
BOOL eBUSPlayerDlg::PreTranslateMessage(MSG* pMsg)
{
    if (WM_KEYFIRST <= pMsg->message && pMsg->message
        <= WM_KEYLAST)
        if (mAccel && ::TranslateAccelerator
            (m_hWnd, mAccel, pMsg))
            return TRUE;

    return CDialog::PreTranslateMessage(pMsg);
}


// ==============================================================================
void eBUSPlayerDlg::OnTimer( UINT_PTR nIDEvent )
{
    if ( nIDEvent == 1 )
    {
        // Periodically refresh the status, helps catching
        // the last image in a sequence if it is filtered out
        // by the display frame rate limiter
        SendMessage( WM_IMAGEDISPLAYED );
    }

    CDialog::OnTimer( nIDEvent );
}


// ==========================================================================
void eBUSPlayerDlg::OnDestroy()
{
    CDialog::OnDestroy();
}


// =============================================================================
void eBUSPlayerDlg::OnCbnSelchangeComboSource()
{
    SendMessage( WM_UPDATESOURCE );
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnAcquisitionStateChanged( WPARAM wParam, LPARAM lParam )
{
    // In case the event has been received after the object has been released
    if ( mPlayer->IsControlledTransmitter() )
    {
        EnableControls( mPlayer->IsDeviceConnected() );
    }

    return 0;
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnUpdateSource( WPARAM wParam, LPARAM lParam )
{
    // Since we get here through the message queue, things may have happened since the message was posted
    if ( !mPlayer->IsDeviceConnected() )
    {
        return 0;
    }

    // Get the new source
    DWORD_PTR lNewSource = mSourceCombo.GetItemData( mSourceCombo.GetCurSel() );

    // Change source, returns OK if changed
    PvResult lResult = mPlayer->ChangeSource( lNewSource );
    if ( lResult.IsOK() )
    {
        // Clear the display
        mDisplay.Clear();

        EnableControls( TRUE );

        // Make sure we update the acquisition mode combo box
        SendMessage( WM_UPDATEACQUISITIONMODE );
    }

    return 0;
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnUpdateSources( WPARAM wParam, LPARAM lParam )
{
    if ( !mPlayer->IsControlledTransmitter() )
    {
        mPlayer->ResetUpdatingSources();
        return 0;
    }

    ComboItemVector lSources;
    mPlayer->GetSources( lSources );

    mSourceCombo.ResetContent();
    ComboItemVector::iterator lIt = lSources.begin();
    while ( lIt != lSources.end() )
    {
        mSourceCombo.AddString( lIt->mName );
        int lIndex = mSourceCombo.GetCount() - 1;

        mSourceCombo.SetItemData( lIndex, static_cast<DWORD_PTR>( lIt->mValue ) );
        if ( lIt->mSelected )
        {
            mSourceCombo.SetCurSel( lIndex );
        }

        lIt++;
    }

    // If we have a preferred selection, attempt to find, select it
    if ( ( lParam >= 0 ) && ( mSourceCombo.GetCount() > 0 ) )
    {
        for ( int i = 0; i < mSourceCombo.GetCount(); i++ )
        {
            if ( mSourceCombo.GetItemData( i ) == static_cast<DWORD_PTR>( lParam ) )
            {
                mSourceCombo.SetCurSel( i );
                SendMessage( WM_UPDATESOURCE );
            }
        }
    }

    // If no selection, select first available
    if ( ( mSourceCombo.GetCount() > 0 ) && ( mSourceCombo.GetCurSel() < 0 ) )
    {
        mSourceCombo.SetCurSel( 0 );
        SendMessage( WM_UPDATESOURCE );
    }

    mPlayer->ResetUpdatingSources();

    return 0;
}


// =============================================================================
LRESULT eBUSPlayerDlg::OnUpdateAcquisitionModes( WPARAM wParam, LPARAM lParam )
{
    if ( !mPlayer->IsControlledTransmitter() )
    {
        return 0;
    }

    // Get acquisition modes from model
    ComboItemVector lModes;
    bool lWritable = false;
    mPlayer->GetAcquisitionModes( lModes, lWritable );

    // Reset combo content
    mModeCombo.ResetContent();

    // Add acquisition modes, select current
    ComboItemVector::const_iterator lIt = lModes.begin();
    while ( lIt != lModes.end() )
    {
        mModeCombo.AddString( lIt->mName );

        int lIndex = mModeCombo.GetCount() - 1;
        mModeCombo.SetItemData( lIndex, static_cast<DWORD_PTR>( lIt->mValue ) );
        if ( lIt->mSelected )
        {
            mModeCombo.SetCurSel( lIndex );
        }

        lIt++;
    }

    // Is it writable?
    mModeCombo.EnableWindow( lWritable ? TRUE : FALSE );

    return 0;
}


// =============================================================================
// wParam is a flag that when set to one resets the BOOL used to prevent
// reentry with the GenApi OnParameterUpdate.
//
// When not called from GenApi OnParameterUpdate (Send or PostMessage) the 
// flag is left alone (not reset) in case there is a OnParameterUpdate message 
// already in the message queue that would depend on it being set.
//
LRESULT eBUSPlayerDlg::OnUpdateAcquisitionMode( WPARAM wParam, LPARAM lParam )
{
    if ( !mPlayer->IsControlledTransmitter() )
    {
        // Not ready yet...
        if ( wParam == 1 )
        {
            mPlayer->ResetUpdatingAcquisitionMode();
        }

        return 0;
    }

    ComboItem lMode;
    bool lWritable = false;
    mPlayer->GetCurrentAcquisitionMode( lMode, lWritable );

    for ( int i = 0; i < mModeCombo.GetCount(); i++ )
    {
        int64_t lValue = static_cast<int64_t>( mModeCombo.GetItemData( i ) );
        if ( lMode.mSelected && ( lValue == lMode.mValue ) )
        {
            mModeCombo.SetCurSel( i );
            break;
        }
    }

    mModeCombo.EnableWindow( lWritable );

    if ( wParam == 1 )
    {
        mPlayer->ResetUpdatingAcquisitionMode();
    }

    return 0;
}


// =============================================================================
void eBUSPlayerDlg::OnMove(int x, int y)
{
    CDialog::OnMove(x, y);
}


