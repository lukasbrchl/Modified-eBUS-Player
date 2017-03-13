// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "eBUSPlayerShared.h"
#include "Player.h"

#include "DeviceGEV.h"
#include "DeviceU3V.h"
#include "StreamGEV.h"
#include "StreamU3V.h"

#include "ChangeSourceTask.h"
#include "ConnectTask.h"
#include "DisconnectTask.h"
#include "LoadTask.h"
#include "SaveTask.h"

#include <PvSystem.h>
#include <PvStreamInfo.h>
#include <PvNetworkAdapter.h>

#include <sstream>
#include <assert.h>
#include <iomanip>


#define TAG_DISPLAYOPTIONS ( "displayoptions" )
#define TAG_ACTIVESOURCE ( "activesource" )
#define TAG_COMMUNICATIONPARAMSGEV ( "communicationparamsgev" )
#define TAG_COMMUNICATIONPARAMSU3V ( "communicationparamsu3v" )

#define SERIALBRIDGE_PERSISTENCE_PREFIX ( "SerialBridge" )

#define LINK_RECOVERY_ENABLED ( "LinkRecoveryEnabled" )

#define NA_STRING ( "N/A" )


#ifdef _AFXDLL
    IMPLEMENT_DYNAMIC( Player, CObject )
#endif // _AFXDLL


PvDeviceGEV Player::sDefaultDeviceGEV;
PvDeviceU3V Player::sDefaultDeviceU3V;
BufferOptions Player::sDefaultBufferOptions;


///
/// \brief Constructor
///

Player::Player( IPlayerController *aController, IPvDisplayAdapter *aDisplay )
    : mController( aController )
    , mDevice( NULL )
    , mStream( NULL )
    , mDisplayThread( NULL )
#ifdef SERIALBRIDGE
    , mSerialBridgeManagerWnd( NULL )
#endif
    , mImageFiltering( NULL )
    , mImageSaving( NULL )
    , mChangingSource( false )
    , mPreferredSource( -1 )
{
	m_NetworkManager = new NetworkManager("localhost", 27015);
	m_NetworkManager->initWinSock();
    mImageFiltering = new ImageFiltering;
    mImageSaving = new ImageSaving( mImageFiltering, m_NetworkManager);
    mDisplayThread = new DisplayThread( aDisplay, mImageFiltering, mImageSaving, &mLogBuffer);
    
    // Initialize buffer options from defaults
    mBufferOptions = sDefaultBufferOptions;

	
}


///
/// \brief Destructor
///

Player::~Player()
{	if (m_NetworkManager != NULL) {
		delete m_NetworkManager;
		m_NetworkManager = NULL;
	}
    // Just in case...
    DisconnectTaskHandler( NULL );

    if ( mDisplayThread != NULL )
    {
        delete mDisplayThread;
        mDisplayThread = NULL;
    }

    if ( mImageSaving != NULL )
    {
        delete mImageSaving;
        mImageSaving = NULL;
    }

    if ( mImageFiltering != NULL )
    {
        delete mImageFiltering;
        mImageFiltering = NULL;
    }
}


///
/// Saves the application configuration to a file
///

PvResult Player::SaveConfig( PvConfigurationWriter *aWriter, PvStringList &aErrorList, bool aSaveConnectedDevice )
{
    if ( !aSaveConnectedDevice )
    {
        // Save everything but the connection: fast, no need for task/progress
        return SaveConfigTaskHandler( NULL, aWriter, aErrorList, false );
    }

    SaveTask lTask( this, aWriter, aErrorList );

    IProgress *lProgress = mController->CreateProgressDialog();
    lProgress->SetTitle( "Configuration Saving Progress" );
    lProgress->RunTask( &lTask );

    delete lProgress;
    lProgress = NULL;

    return lTask.GetResult();
}


///
/// Saves the application configuration to a file
///

PvResult Player::SaveConfigTaskHandler( IProgress *aProgress, PvConfigurationWriter *aWriter, PvStringList &aErrorList, bool aSaveConnectedDevice )
{
    SETSTATUS( "Saving application setup..." );

    mSetup.Save( aWriter );

    SETSTATUS( "Saving imaging filtering configuration..." );

    mImageFiltering->Save( aWriter );

    SETSTATUS( "Saving imaging saving configuration..." );

    mImageSaving->Save( aWriter );

    SETSTATUS( "Saving event monitor configuration..." );

    mLogBuffer.Save( aWriter );

    SETSTATUS( "Saving buffer options..." );

    mBufferOptions.Save( aWriter );

    SETSTATUS( "Saving display options..." );

    PvPropertyList lPropertyList;
    mDisplayThread->Save( lPropertyList );
    aWriter->Store( &lPropertyList, TAG_DISPLAYOPTIONS );

    if ( aSaveConnectedDevice )
    {
        mLogBuffer.EnableGenICamMonitoring( false );

        if ( IsDeviceConnected() )
        {
            SETSTATUS( "Saving device configuration..." );

            // Save device configuration
            aWriter->SetErrorList( &aErrorList, "Device state save error. " );
            mDevice->Save( aWriter );

            // Save currently selected source
            if ( IsDeviceConnected() && mDevice->IsMultiSourceTransmitter() )
            {
                SETSTATUS( "Saving selected source..." );

                int64_t lSource = mDevice->GetSelectedSource();

                std::stringstream lSS;
                lSS << lSource;

                PvString lSourceStr( lSS.str().c_str() );

                aWriter->Store( lSourceStr, TAG_ACTIVESOURCE );
            }

#ifdef SERIALBRIDGE
            PvResult lResult = SaveSerialBridge( aProgress, aWriter, aErrorList );
            if ( !lResult.IsOK() )
            {
                return lResult;
            }
#endif // SERIALBRIDGE
        }

        // Save stream
        if ( IsStreamOpened() )
        {
            SETSTATUS( "Saving stream configuration..." );

            aWriter->SetErrorList( &aErrorList, "Stream state save error. " );
            mStream->Save( aWriter );
        }

        mLogBuffer.EnableGenICamMonitoring( true );
    }
    else
    {
        SETSTATUS( "Saving default communication parameters..." );
    }

    return PvResult::Code::OK;
}


///
/// \brief Opens a configuration
///

PvResult Player::OpenConfig( PvConfigurationReader *aReader, PvStringList &aErrorList, bool aShowProgress )
{
    if ( !aShowProgress )
    {
        // Save everything but the connection: fast, no need for task/progress
        return OpenConfigTaskHandler( NULL, aReader, aErrorList );
    }

    LoadTask lTask( this, aReader, aErrorList );

    IProgress *lProgress = mController->CreateProgressDialog();
    lProgress->SetTitle( "Configuration Restore Progress" );
    lProgress->RunTask( &lTask );

    delete lProgress;
    lProgress = NULL;

    UpdateController();

    return lTask.GetResult();
}


///
/// \brief Open configuration task handler
///

PvResult Player::OpenConfigTaskHandler( IProgress *aProgress, PvConfigurationReader *aReader, PvStringList &aErrorList )
{
    SETSTATUS( "Restoring application setup..." );

    mSetup.Load( aReader );

    SETSTATUS( "Restoring imaging filtering configuration..." );

    mImageFiltering->Load( aReader );

    SETSTATUS( "Restoring event monitor configuration..." );

    mLogBuffer.Load( aReader );

    if ( ( aReader->GetDeviceCount() > 0 ) || ( aReader->GetStreamCount() > 0 ) )
    {
        // If we're connected, just apply the settings. Otherwise connect from the settings in the persistence file.
        if ( IsDeviceConnected() )
        {
            if ( IsControlledTransmitter() )
            {   
                SETSTATUS( "Restoring device configuration..." );

                // Restore device properties
                aReader->SetErrorList( &aErrorList, "Device restore error. " );
                mDevice->Load( aReader );

#ifdef SERIALBRIDGE
                PvResult lResult = LoadSerialBridge( aProgress, aReader, aErrorList );
                if ( !lResult.IsOK() )
                {
                    return lResult;
                }
#endif // SERIALBRIDGE
            }

            uint16_t lChannel = mSetup.GetDefaultChannel();

            if ( IsSetupRoleDataReceiver() )
            {
                SETSTATUS( "Restoring streaming configuration..." );

                aReader->SetErrorList( &aErrorList, "Stream restore error. " );
                mStream->Load( aReader );

                // Obtain the proper channel from the stream we restored
                lChannel = mStream->GetChannel();
            }

            // Now that the stream is opened, set the destination on the device
            if ( IsSetupRoleController() )
            {
                if ( mStream != NULL )
                {
                    SETSTATUS( "Setting stream destination..." );

                    // Set stream destination
                    mDevice->SetStreamDestination( &mSetup, mStream, lChannel );
                }
            }
        }
        else
        {
            PvResult lResult = ConnectTaskHandler( aProgress, NULL, aReader, &aErrorList );
            if ( !lResult.IsOK() )
            {
                // Adapted messsage for not found
                return PvResult( PvResult::Code::NOT_FOUND, "Unable to connect the device. It is either absent or already in use." );
            }
        }

        if ( IsDeviceConnected() && mDevice->IsMultiSourceTransmitter() )
        {
            PvString lSourceStr;
            if ( aReader->Restore( TAG_ACTIVESOURCE, lSourceStr ).IsOK() )
            {
                // Here we just save the preferred source, it will be selected later
                std::stringstream lSS( lSourceStr.GetAscii() );
                lSS >> mPreferredSource;
            }
        }
    }

    SETSTATUS( "Restoring display configuration..." );

    PvPropertyList lPropertyList;
    aReader->Restore( TAG_DISPLAYOPTIONS, &lPropertyList );
    mDisplayThread->Load( lPropertyList );

    SETSTATUS( "Restoring image saving configuration..." );

    mImageSaving->Load( aReader );

    SETSTATUS( "Restoring buffer options..." );

    mBufferOptions.Load( aReader );
    if ( IsStreamOpened() )
    {
        mBufferOptions.Apply( mStream->GetPipeline() );
    }

    return PvResult::Code::OK;
}


///
/// \brief Application connection operation
///

PvResult Player::Connect( const PvDeviceInfo *aDI, PvConfigurationReader *aCR )
{
    ConnectTask lTask( this, aDI, aCR );

    IProgress *lProgress = mController->CreateProgressDialog();
    lProgress->SetTitle( "Connection Progress" );
    lProgress->RunTask( &lTask );

    delete lProgress;
    lProgress = NULL;

    UpdateController();

    return lTask.GetResult();
}


///
/// \brief Connect task handler
///

PvResult Player::ConnectTaskHandler( IProgress *aProgress, const PvDeviceInfo *aDI, PvConfigurationReader *aCR, PvStringList *aErrorList )
{
    assert( aDI != NULL || aCR != NULL );
    if ( aDI == NULL && aCR == NULL )   
    {
        return PvResult::Code::INVALID_PARAMETER;
    }

    DisconnectTaskHandler( aProgress );

    mPreferredSource = -1;

    uint16_t lChannel = mSetup.GetDefaultChannel();
    PvResult lResult;

    if ( aDI != NULL )
    {
        if ( IsSetupRoleController() )
        {
            SETSTATUS( "Connecting device..." );

            PvString lCommunicationParameters;
            GetDefaultCommunicationParameters( aDI, lCommunicationParameters );

            mDevice = Device::Create( aDI, mController, &mLogBuffer );        
            lResult = mDevice->Connect( aProgress, &mSetup, aDI, lChannel, lCommunicationParameters );
            if ( !lResult.IsOK() )
            {
                DisconnectTaskHandler( aProgress );
                return lResult;
            }
        }

        if ( IsSetupRoleDataReceiver() )
        {
            SETSTATUS( "Opening stream..." );

            mStream = Stream::Create( aDI, mController );
            lResult = mStream->Open( &mSetup, aDI, lChannel );
            if ( !lResult.IsOK() )
            {
                DisconnectTaskHandler( aProgress );
                return lResult;
            }
        }
    }
    else if ( aCR != NULL )
    {
#ifdef SERIALBRIDGE
        if ( mSerialBridgeManagerWnd == NULL )
        {
            mSerialBridgeManagerWnd = new SerialBridgeManagerWnd( mController );
        }
#endif // SERIALBRIDGE

        if ( IsSetupRoleController() )
        {   
            SETSTATUS( "Connecting and restoring device configuration..." );

            // 1st try with a GEV device
            assert( mDevice == NULL );
            mDevice = new DeviceGEV( mController, &mLogBuffer );

            lResult = aCR->Restore( 0, mDevice->GetDevice() );
            if ( !lResult.IsOK() )
            {
                // Now try a U3V device
                delete mDevice;
                mDevice = new DeviceU3V( mController, &mLogBuffer );

                lResult = aCR->Restore( 0, mDevice->GetDevice() );
                if ( !lResult.IsOK() )
                {
                    DisconnectTaskHandler( aProgress );
                    return lResult;
                }
            }
        }

#ifdef SERIALBRIDGE
        lResult = LoadSerialBridge( aProgress, aCR, *aErrorList );
#endif // SERIALBRIDGE

        if ( IsSetupRoleDataReceiver() )
        {
            SETSTATUS( "Opening and restoring stream configuration..." );

            // 1st try with a GEV stream
            assert( mStream == NULL );
            mStream = new StreamGEV( mController );

            lResult = aCR->Restore( 0, mStream->GetStream() );
            if ( !lResult.IsOK() )
            {
                // Now try with a U3V stream
                delete mStream;
                mStream = new StreamU3V( mController );

                lResult = aCR->Restore( 0, mStream->GetStream() );
                if ( !lResult.IsOK() )
                {
                    DisconnectTaskHandler( aProgress );
                    return lResult;
                }
            }

            // Obtain the proper channel from the stream we restored
            lChannel = mStream->GetChannel();
        }
    }

    PvSystem lSystem;
    const PvDeviceInfo *lDI = aDI;

    if ( lDI == NULL )
    {
        SETSTATUS( "Retrieving device information..." );

        // Get IP address or GUID of the device of interest
        std::string lInfo;
        if ( IsDeviceConnected() )
        {
            lInfo = mDevice->GetFindString();
        }

        if ( lInfo.length() > 0 )
        {
            // Attempt to get a device info
            lResult = lSystem.FindDevice( lInfo.c_str(), &lDI );
            if ( !lResult.IsOK() )
            {
                DisconnectTaskHandler( aProgress );
                return lResult;
            }
        }
    }

    // Now that we're sure to have a DI, set device class
    if ( IsDeviceConnected() && ( lDI != NULL ) )
    {
        mDevice->SetClass( lDI->GetClass() );
    }

    if ( IsDeviceConnected() && mDevice->IsTransmitter() ) 
    {
        SETSTATUS( "Setting stream destination..." );

        lResult = mDevice->SetStreamDestination( &mSetup, mStream, lChannel );
        if ( !lResult )
        {
            DisconnectTaskHandler( aProgress );
            return lResult;
        }
    }

    // Update device attributes
    UpdateAttributes( lDI );

    if ( IsDeviceConnected() )
    {
#ifdef SERIALBRIDGE
        // Create serial bridge manager, if needed (on persistence load, it may already be there)
        if ( mSerialBridgeManagerWnd == NULL )
        {
            mSerialBridgeManagerWnd = new SerialBridgeManagerWnd( mController );
            mSerialBridgeManagerWnd->SetDevice( mDevice->GetDeviceAdapter() );
        }
#endif // SERIALBRIDGE

        PvStream *lStream = NULL;
        if ( IsStreamOpened() )
        {
            lStream = mStream->GetStream();
        }

        mDevice->CompleteConnect( lStream );
    }

    if ( IsStreamOpened() )
    {
        SETSTATUS( "Configuring streaming buffers..." );
        mBufferOptions.Apply( mStream->GetPipeline() );

        SETSTATUS( "Starting display thread..." );
        StartStreaming();
    }

    return PvResult::Code::OK;
}


///
/// \brief Application connection operation
///

void Player::Disconnect()
{
    mImageSaving->NotifyStreamingStop();

    DisconnectTask lTask( this );

    IProgress *lProgress = mController->CreateProgressDialog();
    lProgress->SetTitle( "Disconnect Progress" );
    lProgress->RunTask( &lTask );

    delete lProgress;
    lProgress = NULL;
}
 

///
/// \brief Application connection operation task handler
///

PvResult Player::DisconnectTaskHandler( IProgress *aProgress )
{
    if ( IsStreaming() )
    {
        Stop();
    }

    if ( mDevice != NULL )
    {
        mDevice->FreeAcquisitionStateManager();
    }

    mStreamMutex.Lock();
    if ( mStream != NULL )
    {
        SETSTATUS( "Closing stream..." );

        delete mStream;
        mStream = NULL;
    }
    mStreamMutex.Unlock();

#ifdef SERIALBRIDGE
    if ( mSerialBridgeManagerWnd != NULL )
    {
        SETSTATUS( "Closing serial bridge manager..." );

        delete mSerialBridgeManagerWnd;
        mSerialBridgeManagerWnd = NULL;
    }
#endif // SERIALBRIDGE

    if ( IsDeviceConnected() )
    {
        SETSTATUS( "Disconnecting device..." );

        delete mDevice;
        mDevice = NULL;
    }

    mDeviceAttributes.Reset();

    return PvResult::Code::OK;
}


///
/// \brief Change application source
///

PvResult Player::ChangeSource( int64_t aNewSource )
{
    if ( !IsDeviceConnected() )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    // Get current source, channel
    int64_t lCurrentSource = mDevice->GetCurrentSource();

    // Is source already active?
    if ( mChangingSource || ( aNewSource == lCurrentSource ) )
    {
        return PvResult::Code::ABORTED;
    }

    mChangingSource = true;

    ChangeSourceTask lTask( this, aNewSource );

    IProgress *lProgress = mController->CreateProgressDialog();
    lProgress->SetTitle( "Source Change Progress" );
    lProgress->RunTask( &lTask );

    delete lProgress;
    lProgress = NULL;

    mChangingSource = false;

    return lTask.GetResult();
}


///
/// \brief Change application source
///

PvResult Player::ChangeSourceTaskHandler( IProgress *aProgress, int64_t aNewSource )
{
    if ( !IsDeviceConnected() )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    StreamGEV *lStreamGEV = dynamic_cast<StreamGEV *>( mStream );
    DeviceGEV *lDeviceGEV = dynamic_cast<DeviceGEV *>( mDevice );

    // Get current channel
    uint16_t lCurrentChannel = mDevice->GetCurrentSourceChannel();

    SETSTATUS( "Stopping acquisition..." );

    // Stop acquisition
    mDevice->StopAcquisition();

    SETSTATUS( "Resetting stream destination..." );

    // Reset stream destination
    lDeviceGEV->ResetStreamDestination( lCurrentChannel );

    SETSTATUS( "Closing stream..." );

    // Close stream
    if ( IsStreamOpened() )
    {
        mStream->Close();
    }

    if ( lDeviceGEV != NULL )
    {
        // Obtain the local IP address and device IP address
        std::string lDeviceIPAddress = lDeviceGEV->GetIPAddress();

        SETSTATUS( "Selecting new source..." );

        // Change the source on the controller
        mDevice->ChangeSource( aNewSource );
        lCurrentChannel = mDevice->GetCurrentSourceChannel();

        SETSTATUS( "Re-opening stream..." );

        // Re-open the stream
        PvStream *lStream = NULL;
        if ( lStreamGEV != NULL )
        {
            lStream = lStreamGEV->GetStream();

            std::string lLocalIPAddress = lStreamGEV->GetLocalIPAddress();
            PvResult lResult = lStreamGEV->Open( &mSetup, lDeviceIPAddress, lLocalIPAddress, lCurrentChannel );
            if ( !lResult.IsOK() )
            {
                return lResult;
            }
        }

        // Update the acquisition manager
        mDevice->CompleteChangeSource( lStream, aNewSource );

        SETSTATUS( "Setting stream destination..." );

        // Set the stream destination
        mDevice->SetStreamDestination( &mSetup, mStream, lCurrentChannel );

        if ( IsStreamOpened() )
        {
            SETSTATUS( "Restarting display thread..." );
            StartStreaming();
        }
    }

    return PvResult::Code::OK;
}


///
/// \brief Application connection operation
///

void Player::GetControlsEnabled( ControlsState &aState )
{
    if ( !IsDeviceConnected() )
    {
        return;
    }

    mDevice->GetControlsEnabled( aState );   
}


///
/// \brief Returns true if the information in the configuration reader is the same as what we now have
///

bool Player::IsSameAsCurrent( PvConfigurationReader *aReader )
{
    bool lSetupIsTheSame = mSetup.IsTheSame( aReader );
    return ( IsDeviceConnected() || IsStreamOpened() ) && !lSetupIsTheSame;
}


///
/// \brief Returns the streaming status
///

void Player::GetStatusText( PvString &aText, bool &aRecording )
{
    aText = "";
    aRecording = false;

    if ( !IsStreamOpened() )
    {
        return;
    }

    std::stringstream lSS;

    if ( mImageSaving->GetEnabled() )
    {
        aRecording = true;

        std::stringstream lAvgFPS( "N/A" );
        if ( mImageSaving->GetFrames() > 0 )
        {
            lAvgFPS << std::fixed << std::setprecision( 1 ) << mImageSaving->GetFPS();
        }

        PvString lLastError;
        mImageSaving->GetLastError( lLastError );

        lSS << "RECORDING ";
        lSS << mImageSaving->GetFrames() << " images    ";
        lSS << mImageSaving->GetTotalSize() << " MB    ";
        lSS << lAvgFPS.str() << " FPS    ";
        lSS << std::fixed << std::setprecision( 1 ) << mImageSaving->GetMbps() << " Mbps" << "\r\n";
        lSS << lLastError.GetAscii();
    }
    else
    {
        PvStreamInfo lInfo( mStream->GetStream() );
        PvString lStatistics = lInfo.GetStatistics( mDisplayThread->GetFPS() );
        PvString lErrors = lInfo.GetErrors();
        PvString lWarnings = lInfo.GetWarnings( mStream->IsPipelineReallocated() );

        lSS << lStatistics.GetAscii() << "\r\n";
        lSS << lErrors.GetAscii() << "\r\n";
        lSS << lWarnings.GetAscii();
    }

    aText = lSS.str().c_str();
}


///
/// \brief Saves the current image.
///

void Player::SaveCurrentImage()
{
    if ( !IsStreamStarted() )
    {
        return;
    }

    mImageSaving->SaveCurrentImage( mDisplayThread);
}


///
/// \brief Play button operation
///

void Player::Play()
{
    // Try getting payload size from the device
    uint32_t lPayloadSize = 0;
    if ( IsDeviceConnected() )
    {
        lPayloadSize = mDevice->GetPayloadSize();
    }

    // Not payload size? Take the default from the buffer options
    if ( lPayloadSize == 0 )
    {
        lPayloadSize = mBufferOptions.GetBufferSize();
    }

    // Arm streaming
    if ( IsStreamOpened() )
    {
        mStream->Reset( lPayloadSize );
    }

	m_NetworkManager->acceptClient();

    // Instruct controller to start streaming
    if ( IsDeviceConnected() )
    {
        mDevice->StartAcquisition();
    }
}


///
/// \brief Stop button operation
///

void Player::Stop()
{
	m_NetworkManager->shutdownSocket();
    if ( IsDeviceConnected() )
    {
        mDevice->StopAcquisition();
        mImageSaving->NotifyStreamingStop();
    }
}


///
/// \brief Returns true if link recovery is enabled for the connected device
///

bool Player::IsLinkRecoveryEnabled()
{
    if ( !IsDeviceConnected() )
    {
        return false;
    }

    PvGenBoolean *lLinkRecoveryEnabled = mDevice->GetCommunicationParameters()->GetBoolean( LINK_RECOVERY_ENABLED );
    if ( lLinkRecoveryEnabled == NULL )
    {
        return false;
    }

    bool lEnabled = false;
    lLinkRecoveryEnabled->GetValue( lEnabled );

    return lEnabled;
}


///
/// \brief Application notified us that the link is lost.
///

void Player::LinkDisconnected()
{
    if ( IsStreamOpened() )
    {
        StopStreaming();
        mStream->Close();
    }
}


///
/// \brief Perform recovery following a device reconnection event
///

void Player::Recover()
{
    if ( !IsDeviceConnected() )
    {
        return;
    }

    DeviceGEV *lDeviceGEV = dynamic_cast<DeviceGEV *>( mDevice );
    if ( lDeviceGEV != NULL )
    {
        RecoverGEV();
    }

    DeviceU3V *lDeviceU3V = dynamic_cast<DeviceU3V *>( mDevice );
    if ( lDeviceU3V != NULL )
    {
        RecoverU3V();
    }

#ifdef SERIALBRIDGE
    if ( mSerialBridgeManagerWnd != NULL )
    {
        mSerialBridgeManagerWnd->Recover();
    }
#endif // SERIALBRIDGE
}


///
/// \brief Complete recovery of a GEV device
///

void Player::RecoverGEV()
{
    assert( ( mSetup.GetRole() == Setup::RoleCtrlData ) ||
            ( mSetup.GetRole() == Setup::RoleCtrl ) );

    DeviceGEV *lDeviceGEV = dynamic_cast<DeviceGEV *>( mDevice );
    StreamGEV *lStreamGEV = dynamic_cast<StreamGEV *>( mStream );
    
    std::string lIP = lDeviceGEV->GetIPAddress();
    std::string lLocalIP = lDeviceGEV->GetLocalIPAddress();

    int64_t lChannel = mDevice->GetCurrentSourceChannel();
    if ( mSetup.GetRole() == Setup::RoleCtrlData )
    {
        // Open stream
        PvResult lResult = lStreamGEV->Open( &mSetup, lIP, lLocalIP, static_cast<uint16_t>( lChannel ) );
        if ( !lResult.IsOK() )
        {
            return;
        }

        // Stream is opened, now start it
        StartStreaming();
    }

    // Set stream destination
    lDeviceGEV->SetStreamDestination( &mSetup, mStream, (uint16_t)lChannel );

    // Start acquisition, if needed
    if ( mDevice->IsAcquisitionLocked() )
    {
        Stop();
        Play();
    }

    mDevice->ResetRecovery();
}


///
/// \brief Complete recovery of a U3V device
///

void Player::RecoverU3V()
{
    assert( ( mSetup.GetRole() == Setup::RoleCtrlData ) ||
            ( mSetup.GetRole() == Setup::RoleCtrl ) );

    // End point was likely reset, re-open stream
    if ( mStream != NULL )
    {
        StreamU3V *lStreamU3V = dynamic_cast<StreamU3V *>( mStream );
        lStreamU3V->Recover();

        StartStreaming();
    }

    // Start acquisition, if needed
    if ( mDevice != NULL )
    {
        if ( mDevice->IsAcquisitionLocked() )
        {
            Stop();
            Play();
        }
        
        mDevice->ResetRecovery();
    }
}


///
/// \brief Starts streaming
///

void Player::StartStreaming()
{
    if ( !IsStreamOpened() )
    {
        return;
    }

    mStream->Start( mDisplayThread, mImageSaving, &mBufferOptions, GetDeviceParameters() );
    mController->StartTimer();
}


///
/// \brief Starts streaming
///

void Player::StopStreaming()
{
    if ( !IsStreamOpened() )
    {
        return;
    }

    mController->StopTimer();
    mStream->Stop();
}


///
/// \brief Returns communication parameters
///

PvGenParameterArray *Player::GetCommunicationParameters()
{
    if ( !IsDeviceConnected() )
    {
        return NULL;
    }

    return mDevice->GetCommunicationParameters();
}


///
/// \brief Returns device parameters
///

PvGenParameterArray *Player::GetDeviceParameters()
{
    if ( !IsDeviceConnected() )
    {
        return NULL;
    }

    return mDevice->GetParameters();
}


///
/// \brief Returns stream parameters
///

PvGenParameterArray *Player::GetStreamParameters()
{
    if ( !IsStreamOpened() )
    {
        return NULL;
    }

    return mStream->GetParameters();
}


#ifdef SERIALBRIDGE

///
/// \brief Loads serial bridge configuration (bridges and camera configuration)
///

PvResult Player::LoadSerialBridge( IProgress *aProgress, PvConfigurationReader *aReader, PvStringList &aErrorList )
{
    if ( mSerialBridgeManagerWnd != NULL )
    {
        std::string lPrefix = SERIALBRIDGE_PERSISTENCE_PREFIX;
        size_t lPrefixLength = lPrefix.length();

        SETSTATUS( "Restoring serial communication bridge configuration..." );

        PvPropertyList lList;
        aReader->Restore( "SerialBridgeManager", &lList );

        // Let the serial bridge manager to perform the load
        mSerialBridgeManagerWnd->SetDevice( mDevice->GetDeviceAdapter() );
        mSerialBridgeManagerWnd->Load( lList, aErrorList );

        // Go through ports, try finding persistence data for node maps (if any)
        for ( uint32_t i = 0; i < mSerialBridgeManagerWnd->GetPortCount(); i++ )
        {
            PvGenParameterArray *lNodeMap = mSerialBridgeManagerWnd->GetGenParameterArray( i );
            if ( lNodeMap != NULL )
            {
                PvString lPortName = mSerialBridgeManagerWnd->GetPortName( i );

                std::string lStatus( "Restoring configuration of camera on " );
                lStatus += lPortName.GetAscii();
                lStatus += "...";
                SETSTATUS( lStatus );

                std::string lStorageName( lPortName.GetAscii() );
                lStorageName += "Camera";

                std::string lPrefix( "Camera (" );
                lPrefix += lPortName.GetAscii();
                lPrefix += ") restore error.";

                aReader->SetErrorList( &aErrorList, lPrefix.c_str() );
                aReader->Restore( lStorageName.c_str(), lNodeMap );
            }
        }
    }

    return PvResult::Code::OK;
}


///
/// \brief Saves a serial bridge configuration (bridges and camera configuration)
///

PvResult Player::SaveSerialBridge( IProgress *aProgress, PvConfigurationWriter *aWriter, PvStringList &aErrorList )
{
    if ( mSerialBridgeManagerWnd != NULL )
    {
        SETSTATUS( "Saving serial communication bridge configuration..." );

        // Get persistence data
        PvPropertyList lList;
        PvResult lResult = mSerialBridgeManagerWnd->Save( lList );
        if ( !lResult.IsOK() )
        {
            return lResult;
        }

        // Write to persistence file
        aWriter->Store( &lList, "SerialBridgeManager" );

        // Save camera configuration (if any)
        for ( uint32_t i = 0; i < mSerialBridgeManagerWnd->GetPortCount(); i++ )
        {
            PvGenParameterArray *lNodeMap = mSerialBridgeManagerWnd->GetGenParameterArray( i );
            if ( lNodeMap != NULL )
            {
                PvString lPortName = mSerialBridgeManagerWnd->GetPortName( i );

                std::string lStatus( "Saving configuration of camera on " );
                lStatus += lPortName.GetAscii();
                lStatus += "...";

                SETSTATUS( lStatus );

                std::string lStorageName( lPortName.GetAscii() );
                lStorageName += "Camera";

                std::string lPrefix( "Camera (" );
                lPrefix += lPortName.GetAscii();
                lPrefix += ") save error.";

                aWriter->SetErrorList( &aErrorList, lPrefix.c_str() );
                aWriter->Store( lNodeMap, lStorageName.c_str() );
            }
        }
    }
    
    return PvResult::Code::OK;
}

#endif // SERIALBRIDGE


///
/// \brief Returns true if the buffer options are in sync with the pipeline configuration
///

bool Player::DoBufferOptionsRequireApply()
{
    if ( !IsStreamOpened() )
    {
        return true;
    }

    return mBufferOptions.HasChanged( mStream->GetPipeline() );
}


///
/// \brief Applies the buffer options to the pipeline
///

PvResult Player::ApplyBufferOptions()
{
    if ( !IsStreamOpened() )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    return mBufferOptions.Apply( mStream->GetPipeline() );
}


///
/// \brief Resets a flag preventing re-entry
///

void Player::ResetUpdatingSources()
{
    if ( IsDeviceConnected() )
    {
        mDevice->ResetUpdatingSources();
    }
}


///
/// \brief Resets a flag preventing re-entry
///

void Player::ResetUpdatingAcquisitionMode()
{
    if ( IsDeviceConnected() )
    {
        mDevice->ResetUpdatingAcquisitionMode();
    }
}


///
/// \brief Resets streaming statistics
///

void Player::ResetStreamingStatistics()
{
	if ( IsStreamOpened() )
	{
		mStream->ResetStatistics();
	}
}


///
/// \brief Returns the default name of the GenCIAm XML file of the device
///

PvString Player::GetDeviceXMLDefaultName()
{
    if ( !IsDeviceConnected() )
    {
        return "";
    }

    return mDevice->GetDeviceXMLDefaultName();
}


///
/// \brief Saves the GenICam XML file of the device
///

PvResult Player::SaveDeviceXML( const PvString &aFilename )
{
    if ( !IsDeviceConnected() )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    return mDevice->SaveDeviceXML( aFilename );

}


///
/// \brief Returns the PvDevice used for device control.
///
/// We attempt not to use it except where necessary.
///

PvDevice *Player::GetPvDevice()
{
    if ( !IsDeviceConnected() )
    {
        return NULL;
    }

    return mDevice->GetDevice();
}


///
/// \brief Returns the adapter to the PvDevice used for device control.
///

IPvDeviceAdapter *Player::GetPvDeviceAdapter()
{
    if ( !IsDeviceConnected() )
    {
        return NULL;
    }

    return mDevice->GetDeviceAdapter();
}


///
/// \brief Returns the current acquisition mode
///

void Player::GetCurrentAcquisitionMode( ComboItem &aMode, bool &aWritable )
{
    if ( !IsDeviceConnected() )
    {
        return;
    }

    mDevice->GetCurrentAcquisitionMode( aMode, aWritable );
}


///
/// \brief Returns all acquisition modes for the device
///

void Player::GetAcquisitionModes( ComboItemVector &aModes, bool &aWritable )
{
    if ( !IsDeviceConnected() )
    {
        return;
    }


    mDevice->GetAcquisitionModes( aModes, aWritable );
}


///
/// \brief Sets a new acquisition mode on the device
///

PvResult Player::SetAcquisitionMode( int64_t aNewMode )
{
    if ( !IsDeviceConnected() )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    return mDevice->SetAcquisitionMode( aNewMode );
}


///
/// \brief Returns all sources supported by the device
///

void Player::GetSources( ComboItemVector &aSources )
{
    if ( !IsDeviceConnected() )
    {
        return;
    }

    mDevice->GetSources( aSources );
}


///
/// \brief Returns the default communication parameters in a string
///

void Player::GetDefaultCommunicationParameters( const PvDeviceInfo *aDeviceInfo, PvString &aParameters )
{
    PvConfigurationWriter lWriter;

    PvGenParameterArray *lParameters = NULL;
    switch ( aDeviceInfo->GetType() )
    {
    case PvDeviceInfoTypeGEV:
        lParameters = sDefaultDeviceGEV.GetCommunicationParameters();
        break;

    case PvDeviceInfoTypeU3V:
        lParameters = sDefaultDeviceU3V.GetCommunicationParameters();
        break;

    default:
        assert( 0 );
    }

    if ( lParameters == NULL )
    {
        return;
    }

    lWriter.Store( lParameters, "Communication" );
    lWriter.SaveToString( aParameters );
}


///
///  \brief Updates the device attributes from a device info
///

void Player::UpdateAttributes( const PvDeviceInfo *aDI )
{
    if ( aDI == NULL )
    {
        return;
    }

    mDeviceAttributes.Reset();

    mDeviceAttributes.mVendor = aDI->GetVendorName().GetAscii();
    mDeviceAttributes.mModel = aDI->GetModelName().GetAscii();
    mDeviceAttributes.mName = aDI->GetUserDefinedName().GetAscii();

    const PvDeviceInfoGEV *lDeviceInfoGEV = dynamic_cast<const PvDeviceInfoGEV *>( aDI );
    if ( lDeviceInfoGEV != NULL )
    {
        mDeviceAttributes.mIP = lDeviceInfoGEV->GetIPAddress().GetAscii();
        mDeviceAttributes.mMAC = lDeviceInfoGEV->GetMACAddress().GetAscii();
    }
    else
    {
        mDeviceAttributes.mIP = NA_STRING;
        mDeviceAttributes.mMAC = NA_STRING;
    }

    const PvDeviceInfoU3V *lDeviceInfoU3V = dynamic_cast<const PvDeviceInfoU3V *>( aDI );
    if ( lDeviceInfoU3V != NULL )
    {
        mDeviceAttributes.mGUID = lDeviceInfoU3V->GetDeviceGUID().GetAscii();
    }
    else
    {
        mDeviceAttributes.mGUID = NA_STRING;
    }
}


///
/// \brief Is the stream opened?
///

bool Player::IsStreamOpened() 
{ 
    mStreamMutex.Lock();
    bool lOpened = ( mStream != NULL ) && mStream->IsOpened(); 
    mStreamMutex.Unlock();

    return lOpened;
}


///
/// \brief Is the stream started? (display thread running)
///

bool Player::IsStreamStarted() 
{ 
    mStreamMutex.Lock();
    bool lStarted = ( mStream != NULL ) && mStream->IsOpened() && mStream->IsStarted(); 
    mStreamMutex.Unlock();

    return lStarted;
}


///
/// \brief Sends messages to the controller to update itself
///

void Player::UpdateController()
{
    mController->SendMsg( WM_UPDATEACQUISITIONMODES );
    mController->SendMsg( WM_UPDATESOURCES, 0, mPreferredSource );
    mController->SendMsg( WM_UPDATEACQUISITIONMODE );
}


