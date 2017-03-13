// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <IPlayerController.h>
#include <IProgress.h>
#include <Device.h>
#include <Stream.h>
#include <ImageFiltering.h>
#include <Setup.h>

#include <PvDeviceGEV.h>
#include <PvDeviceU3V.h>

#include "NetworkManager.h"

#ifdef SERIALBRIDGE
#include <SerialBridgeManagerWnd.h>
#endif // SERIALBRIDGE


struct DeviceAttributes 
{
    void Reset()
    {
        mVendor = "";
        mModel = "";
        mName = "";
        mIP = "";
        mMAC = "";
        mGUID = "";
    }

    PvString mVendor;
    PvString mModel;
    PvString mName;
    PvString mIP;
    PvString mMAC;
    PvString mGUID;
};


class Player
{
#ifdef _AFXDLL
    DECLARE_DYNAMIC( Player )
#endif // _AFXDLL

public:

    Player( IPlayerController *aController, IPvDisplayAdapter *aDisplay );
    ~Player();

    bool IsControlledTransmitter() { return ( mDevice != NULL ) && mDevice->IsTransmitter(); }
    bool IsDeviceConnected() { return ( mDevice != NULL ) && mDevice->IsConnected(); }
    bool IsStreamOpened();
    bool IsStreamStarted();
    bool IsStreaming() { return IsDeviceConnected() && mDevice->IsStreaming(); }
    bool IsInRecovery() { return IsDeviceConnected() && mDevice->IsInRecovery(); }
    bool IsSerialSupported() { return IsDeviceConnected() && mDevice->IsSerialSupported(); }
    bool IsLinkRecoveryEnabled();

    void ResetSetup() { mSetup.Reset(); }
    Setup::Role GetSetupRole() const { return mSetup.GetRole(); }
    bool IsSetupRoleDataReceiver() const { return mSetup.IsDataReceiver(); }
    bool IsSetupRoleController() const { return mSetup.IsController(); }

#ifdef SERIALBRIDGE
    PvSerialBridgeManagerWnd *GetSerialBridgeManager() { return mSerialBridgeManagerWnd; }
#endif // SERIALBRIDGE

    void GetControlsEnabled( ControlsState &aState );
    void GetDefaultCommunicationParameters( const PvDeviceInfo *aDeviceInfo, PvString &aParameters );
    DeviceAttributes &GetDeviceAttributes() { return mDeviceAttributes; }

    PvResult Connect( const PvDeviceInfo *aDI, PvConfigurationReader *aCR );
    void StartStreaming();
    void Play();
    void Disconnect();
    void StopStreaming();
    void Stop();

    void LinkDisconnected();
    void Recover();

    PvResult ChangeSource( int64_t aNewSource );
    
    Setup *GetSetup() { return &mSetup; }
    LogBuffer *GetLogBuffer() { return &mLogBuffer; }
    DisplayThread *GetDisplayThread() { return mDisplayThread; }
    ImageSaving *GetImageSaving() { return mImageSaving; }
    ImageFiltering *GetImageFiltering() { return mImageFiltering; }
    BufferOptions *GetBufferOptions() { return &mBufferOptions; }

    PvResult OpenConfig( PvConfigurationReader *aReader, PvStringList &aErrorList, bool aShowProgress );
    PvResult SaveConfig( PvConfigurationWriter *aWriter, PvStringList &aErrorList, bool aSaveConnectedDevice );
    bool IsSameAsCurrent( PvConfigurationReader *aReader );

    void GetStatusText( PvString &aText, bool &aRecording );

    void SaveCurrentImage();
    PvString GetDeviceXMLDefaultName();
    PvResult SaveDeviceXML( const PvString &aFilename );
    
    PvDevice *GetPvDevice();
    IPvDeviceAdapter *GetPvDeviceAdapter();

    void GetSources( ComboItemVector &aSources );
    void GetCurrentAcquisitionMode( ComboItem &aMode, bool &aWritable );
    void GetAcquisitionModes( ComboItemVector &aModes, bool &aWritable );
    PvResult SetAcquisitionMode( int64_t aNewMode );

    bool DoBufferOptionsRequireApply();
    PvResult ApplyBufferOptions();

    void ResetUpdatingSources();
    void ResetUpdatingAcquisitionMode();
	void ResetStreamingStatistics();

    void UpdateAttributes( const PvDeviceInfo *aDI );
    void UpdateController();

    static PvGenParameterArray *GetDefaultGEVCommunicationParameters() { return sDefaultDeviceGEV.GetCommunicationParameters(); }
    static PvGenParameterArray *GetDefaultU3VCommunicationParameters() { return sDefaultDeviceU3V.GetCommunicationParameters(); }
    static BufferOptions *GetDefaultBufferOptions() { return &sDefaultBufferOptions; }
    
    PvGenParameterArray *GetCommunicationParameters();
    PvGenParameterArray *GetDeviceParameters();
    PvGenParameterArray *GetStreamParameters();

    PvResult ConnectTaskHandler( IProgress *aProgress, const PvDeviceInfo *aDI, PvConfigurationReader *aCR, PvStringList *aErrorList = NULL );
    PvResult DisconnectTaskHandler( IProgress *aProgress );
    PvResult OpenConfigTaskHandler( IProgress *aProgress, PvConfigurationReader *lReader, PvStringList &aErrorList );
    PvResult SaveConfigTaskHandler( IProgress *aProgress, PvConfigurationWriter *lWriter, PvStringList &aErrorList, bool aSaveConnectedDevice );
    PvResult ChangeSourceTaskHandler( IProgress *aProgress, int64_t aNewSource );

protected:

    void RecoverGEV();
    void RecoverU3V();

#ifdef SERIALBRIDGE
    PvResult LoadSerialBridge( IProgress *aProgress, PvConfigurationReader *aReader, PvStringList &aErrorList );
    PvResult SaveSerialBridge( IProgress *aProgress, PvConfigurationWriter *aWriter, PvStringList &aErrorList );
#endif // SERIALBRIDGE

private:
	NetworkManager * m_NetworkManager;
    IPlayerController *mController;

    Device *mDevice;
    
    Stream *mStream;
    Mutex mStreamMutex;

    Setup mSetup;
    LogBuffer mLogBuffer;
    DisplayThread *mDisplayThread;
    ImageSaving *mImageSaving;
    ImageFiltering *mImageFiltering;
    BufferOptions mBufferOptions;

    static PvDeviceGEV sDefaultDeviceGEV;
    static PvDeviceU3V sDefaultDeviceU3V;
    static BufferOptions sDefaultBufferOptions;

    bool mChangingSource;
    int32_t mPreferredSource;

#ifdef SERIALBRIDGE
    SerialBridgeManagerWnd *mSerialBridgeManagerWnd;
#endif // SERIALBRIDGE

    DeviceAttributes mDeviceAttributes;

};


#endif // __PLAYER_H__

