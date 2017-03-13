// *****************************************************************************
//
//     Copyright (c) 2008, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#pragma once

#include "resource.h"

#include <PvActionCommand.h>


// Specialized CEdit to handle dec and hex
class CHexEdit : public CEdit
{
public:

    CHexEdit() {}
    virtual ~CHexEdit() {}

    void SetValue( uint32_t aValue, bool aHex );
    void SetValue( uint64_t aValue, bool aHex );

    uint32_t GetValueUInt32();
    uint64_t GetValueUInt64();

    void Cut();

protected:

    DECLARE_MESSAGE_MAP()
    afx_msg void OnChar(UINT aChar, UINT aRepCnt, UINT aFlags);
    afx_msg LRESULT OnPaste( WPARAM awParam, LPARAM alParam );
    afx_msg void OnKillFocus( CWnd* pcNewReceiver );

    void ValidateValues();

};


// ActionCommand dialog
class ActionCommandDlg : public CDialog
{
    DECLARE_DYNAMIC( ActionCommandDlg )

public:

    ActionCommandDlg( CWnd* pParent = NULL );
    virtual ~ActionCommandDlg();

    enum { IDD = IDD_ACTIONCOMMAND };

protected:

    virtual void DoDataExchange( CDataExchange* pDX );
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedCheckscheduled();
    afx_msg void OnBnClickedButtonsend();

    CHexEdit mDeviceKey;
    CHexEdit mGroupKey;
    CHexEdit mGroupMask;
    CHexEdit mScheduledTime;
    CButton mRequestAcknowledgesCheckBox;
    CButton mScheduled;
    CStatic mActionTimeLabel;
    CButton mSend;
    CListBox mAcknowledges;
    CCheckListBox mInterfaces;

private:

    PvActionCommand mActionCommand;

};

