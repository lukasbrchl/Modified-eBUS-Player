// *****************************************************************************
//
//     Copyright (c) 2012, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "stdafx.h"
#include "ActionCommandDlg.h"


BEGIN_MESSAGE_MAP(CHexEdit, CEdit)    
    ON_WM_CHAR()
    ON_MESSAGE(WM_PASTE, OnPaste)
    ON_WM_KILLFOCUS()
END_MESSAGE_MAP()


void CHexEdit::SetValue( uint32_t aValue, bool aHex )
{
    CString lTemp;
    if( aHex )
    {
        lTemp.Format( L"0x%08X", aValue );
    }
    else
    {
        lTemp.Format( L"%u", aValue );
    }
    SetWindowTextW( lTemp );
}


void CHexEdit::SetValue( uint64_t aValue, bool aHex )
{
    CString lTemp;
    if( aHex )
    {
        lTemp.Format( L"%016llX", aValue );
    }
    else
    {
        lTemp.Format( L"%llu", aValue );
    }
    SetWindowTextW( lTemp );
}


uint32_t CHexEdit::GetValueUInt32()
{
    uint32_t lValue = 0;
    CString lText;
    bool lHex = false;

    GetWindowText( lText );

    for( int i = 0; i < lText.GetLength(); i++ )
    {
        if( wcschr( L"AaBbCcDdEeFfxX", lText[ i ] ) != NULL )
        {
            lHex = true;
            break;
        }
    }

    if( lHex )
    {
        swscanf_s( lText, L"%x", &lValue ); 
    }
    else
    {
        swscanf_s( lText, L"%u", &lValue ); 
    }

    return lValue;
}


uint64_t CHexEdit::GetValueUInt64()
{
    uint64_t lValue = 0;
    CString lText;
    bool lHex = false;

    GetWindowText( lText );

    for( int i = 0; i < lText.GetLength(); i++ )
    {
        if( wcschr( L"AaBbCcDdEeFfxX", lText[ i ] ) != NULL )
        {
            lHex = true;
            break;
        }
    }

    if( lHex )
    {
        swscanf_s( lText, L"%llx", &lValue ); 
    }
    else
    {
        swscanf_s( lText, L"%llu", &lValue ); 
    }

    return lValue;
}


void CHexEdit::ValidateValues()
{
    // Just validate that we do not need a 0x...
    int lStartChar;
    int lEndChar;
    CString lText;

    GetWindowText( lText );
    GetSel( lStartChar, lEndChar );

    for( int i = 0; i < lText.GetLength(); i++ )
    {
        if( wcschr( L"AaBbCcDdEeFf", lText[ i ] ) != NULL )
        {
            if( lText.Find( L"0x" ) < 0 
                && lText.Find( L"0X" ) < 0)
            {
                if( lText[ 0 ] == L'x' || lText[ 0 ] == L'X' )
                {
                    lText.Insert( 0, L"0" );
                    lStartChar++;
                }
                else
                {
                    lText.Insert( 0, L"0x" );
                    lStartChar += 2;
                }

                SetWindowText( lText );
                SetSel( lStartChar, lStartChar );
                return;
            }
        }
    }
}


void CHexEdit::OnKillFocus( CWnd* pcNewReceiver ) 
{
    ValidateValues();

    CEdit::OnKillFocus( pcNewReceiver );
}


void CHexEdit::Cut()
{
    CEdit::Cut();

    ValidateValues();
}


LRESULT CHexEdit::OnPaste( WPARAM awParam, LPARAM alParam )
{
    int lStartChar;
    int lEndChar;
    CString lText;

    UNUSED_ALWAYS( awParam );
    UNUSED_ALWAYS( alParam );

    GetWindowText( lText );
    GetSel( lStartChar, lEndChar );

    if( lStartChar < 2 
        && ( lText.Find( L"0x" ) == 0 
        || lText.Find( L"0X" ) == 0 ) )
    {
        return 0;
    }

    // Remove the selected item
    if( lEndChar - lStartChar )
    {
        lText.Delete( lStartChar, lEndChar - lStartChar );
    }

    // Now paste the element from the clipboard
    CString lClipboardText;
    if( ::IsClipboardFormatAvailable( CF_UNICODETEXT ) )
    {
        // open the clipboard to get clipboard text
        if( ::OpenClipboard( m_hWnd ) )
        {
            HANDLE lClipBrdData = NULL;
            if( ( lClipBrdData = ::GetClipboardData( CF_UNICODETEXT ) ) != NULL )
            {
                LPWSTR lpClipBrdText = ( LPWSTR )::GlobalLock( lClipBrdData );
                if (lpClipBrdText)
                {
                    lClipboardText = lpClipBrdText;
                    ::GlobalUnlock( lClipBrdData );
                }
            }
            VERIFY( ::CloseClipboard() );
        }
    }
    else if( ::IsClipboardFormatAvailable( CF_TEXT ) )
    {
        // open the clipboard to get clipboard text
        if( ::OpenClipboard( m_hWnd ) )
        {
            HANDLE lClipBrdData = NULL;
            if( ( lClipBrdData = ::GetClipboardData( CF_TEXT ) ) != NULL )
            {
                LPCSTR lpClipBrdText = ( LPCSTR )::GlobalLock( lClipBrdData );
                if (lpClipBrdText)
                {
                    PvString lTextToConvert( lpClipBrdText );
                    lClipboardText = lTextToConvert.GetUnicode();
                    ::GlobalUnlock( lClipBrdData );
                }
            }
            VERIFY( ::CloseClipboard() );
        }
    }
    
    int i = 0;
    if( lClipboardText.Find( L"0x" ) == 0 
        || lClipboardText.Find( L"0X" ) == 0 )
    {
        i += 2;
    }
    for( ; i < lClipboardText.GetLength(); i++ )
    {
        if( wcschr( L"0123456789AaBbCcDdEeFf", lClipboardText[ i ] ) != NULL )
        {
            lText.Insert( lStartChar, lClipboardText[ i ] );
            lStartChar++;
        }
    }

    if( wcschr( L"AaBbCcDdEeFf", lClipboardText[ i ] ) != NULL )
    {
        if( lText.Find( L"0x" ) < 0 
            && lText.Find( L"0X" ) < 0)
        {
            if( lText[ 0 ] == L'x' || lText[ 0 ] == L'X' )
            {
                lText.Insert( 0, L"0" );
                lStartChar++;
            }
            else
            {
                lText.Insert( 0, L"0x" );
                lStartChar += 2;
            }
        }
    }
    SetWindowText( lText );
    SetSel( lStartChar, lStartChar );
    
    return 0;
}


void CHexEdit::OnChar(UINT aChar, UINT aRepCnt, UINT aFlags )
{    
    CString lText;
    
    if( GetKeyState( VK_CONTROL ) & 0x80000000 ) 
    {
        switch( aChar ) 
        {
            case 0x03:         
                Copy();
                return;
            case 0x16:
                Paste();
                return;
            case 0x18:
                Cut();
                return;
            case 0x1a:
                Undo();
                return;
        }
    }
    else
    {
        switch( aChar )
        {
            case _T('\b'):   
            case 10:   
            case 13:      
                CEdit::OnChar( aChar, aRepCnt, aFlags );
                break;
            default:

                if( wcschr( L"AaBbCcDdEeFfhHxX", aChar ) != NULL )
                {
                    CString lText;
                    GetWindowText( lText );
                    if( lText.Find( L"0x" ) < 0 
                        && lText.Find( L"0X" ) < 0)
                    {
                        int lStartChar;
                        int lEndChar;

                        GetSel( lStartChar, lEndChar );
                        SetWindowText( L"0x" + lText );
                        SetSel( lEndChar + 2, lEndChar + 2 );
                    }
                }

                if( wcschr( L"0123456789AaBbCcDdEeFf", aChar ) != NULL )
                {
                    CEdit::OnChar( aChar, aRepCnt, aFlags );
                }
                break;
        }
    }
}


IMPLEMENT_DYNAMIC(ActionCommandDlg, CDialog)

ActionCommandDlg::ActionCommandDlg(CWnd* pParent /*=NULL*/)
    : CDialog(ActionCommandDlg::IDD, pParent)
{

}

ActionCommandDlg::~ActionCommandDlg()
{
}


void ActionCommandDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDITDEVICEKEY, mDeviceKey);
    DDX_Control(pDX, IDC_EDITGROUPKEY, mGroupKey);
    DDX_Control(pDX, IDC_EDITGROUPMASK, mGroupMask);
    DDX_Control(pDX, IDC_ACTIONTIME, mScheduledTime);
    DDX_Control(pDX, IDC_REQUESTACKNOWLEDGESCHECKBOX, mRequestAcknowledgesCheckBox);
    DDX_Control(pDX, IDC_CHECKSCHEDULED, mScheduled);
    DDX_Control(pDX, IDC_STATIC_ACTIONTIME_ENABLED, mActionTimeLabel);
    DDX_Control(pDX, IDC_BUTTONSEND, mSend);
    DDX_Control(pDX, IDC_LISTACK, mAcknowledges);
    DDX_Control(pDX, IDC_LIST_INTERFACES, mInterfaces);
}


BEGIN_MESSAGE_MAP(ActionCommandDlg, CDialog)
    ON_BN_CLICKED(IDC_CHECKSCHEDULED, &ActionCommandDlg::OnBnClickedCheckscheduled)
    ON_BN_CLICKED(IDC_BUTTONSEND, &ActionCommandDlg::OnBnClickedButtonsend)
END_MESSAGE_MAP()


BOOL ActionCommandDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString lTemp;
    PvString lDescription;
    PvString lIPAddress;
    PvString lMACAddress;

    // Populate the interface lists
    mInterfaces.ResetContent();
    mInterfaces.SetCheckStyle( BS_AUTOCHECKBOX );
    for( uint32_t i = 0; i < mActionCommand.GetInterfaceCount(); i++ )
    {
        // Format the information about the interface for the display
        mActionCommand.GetInterfaceDescription( i, lDescription );
        mActionCommand.GetInterfaceIPAddress( i, lIPAddress );
        mActionCommand.GetInterfaceMACAddress( i, lMACAddress );
        lTemp.Format( L"%s, %s, %s", 
            lDescription.GetUnicode(), lIPAddress.GetUnicode(), lMACAddress.GetUnicode() );

        // Now add the line to the control  
        mInterfaces.AddString( lTemp );

        bool lEnabled = false;
        mActionCommand.GetInterfaceEnabled( i, lEnabled );
        mInterfaces.SetCheck( i, lEnabled );

    }
    
    // Try to setup the default to all the default value from the class
    mDeviceKey.SetValue( mActionCommand.GetDeviceKey(), true );
    mGroupKey.SetValue( mActionCommand.GetGroupKey(), true );
    mGroupMask.SetValue( mActionCommand.GetGroupMask(), true );

    uint64_t lScheduledTime = mActionCommand.GetScheduledTime();
    mScheduledTime.SetValue( lScheduledTime, false );
    mScheduledTime.EnableWindow( lScheduledTime != 0ULL );
    mActionTimeLabel.EnableWindow( lScheduledTime != 0ULL );
    mScheduled.SetCheck( lScheduledTime != 0ULL );
    
    // We default to requesting acknowledges
    mRequestAcknowledgesCheckBox.SetCheck( BST_CHECKED );

    return TRUE;
}


void ActionCommandDlg::OnBnClickedCheckscheduled()
{
    int lChecked = mScheduled.GetCheck();

    mScheduledTime.EnableWindow( lChecked );
    mActionTimeLabel.EnableWindow( lChecked );
}


void ActionCommandDlg::OnBnClickedButtonsend()
{
    CWaitCursor lWaitCursor;

    mAcknowledges.ResetContent();

    // Now build configure the action command
    mActionCommand.SetDeviceKey( mDeviceKey.GetValueUInt32() );
    mActionCommand.SetGroupKey( mGroupKey.GetValueUInt32() );
    mActionCommand.SetGroupMask( mGroupMask.GetValueUInt32() );
    mActionCommand.SetScheduledTimeEnable( mScheduled.GetCheck() ? true : false );
    mActionCommand.SetScheduledTime( mScheduledTime.GetValueUInt64() );

    // Update the selection of action command
    for( uint32_t i = 0; i < mActionCommand.GetInterfaceCount(); i++ )
    {
        mActionCommand.SetInterfaceEnabled( i, mInterfaces.GetCheck( i ) ? true : false );
    }

    // Now send the command
    bool lRequestAcknowledges = ( mRequestAcknowledgesCheckBox.GetCheck() == BST_CHECKED ) ? true : false;
    PvResult lResult = mActionCommand.Send( 1000U, 0U, lRequestAcknowledges );
    if( lResult.IsOK() )
    {
        CString lTemp;
        PvActionAckStatusEnum lStatus;
        PvString lIPAddress;
        static const wchar_t* sActionAckText[] = {L"OK", L"Late", L"Overflow", L"No ref time" };

        for( uint32_t i = 0; i < mActionCommand.GetAcknowledgementCount(); i++ )
        {
            mActionCommand.GetAcknowledgementStatus( i, lStatus );
            mActionCommand.GetAcknowledgementIPAddress( i, lIPAddress );

            lTemp.Format( L"[ %s ] from %s", sActionAckText[ lStatus ], lIPAddress.GetUnicode() );
            mAcknowledges.AddString( lTemp );
        }
    }

}


void ActionCommandDlg::OnOK()
{
    // Do nothing, just ensure not calling the parent
}


void ActionCommandDlg::OnCancel()
{
    DestroyWindow();
}
