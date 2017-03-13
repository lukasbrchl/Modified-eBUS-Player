// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "eBUSPlayerShared.h"
#include "DeviceGEV.h"
#include "StreamGEV.h"

#include <PvDeviceGEV.h>
#include <PvDeviceInfoGEV.h>

#include <assert.h>
#include <sstream>


#define AUTO_NEGOTIATION ( "AutoNegotiation" )
#define GEV_SCPS_PACKET_SIZE ( "GevSCPSPacketSize" )
#define GEV_STREAM_CHANNEL_COUNT ( "GevStreamChannelCount" )
#define DEFAULT_PACKET_SIZE ( "DefaultPacketSize" )
#define SOURCE_STREAM_CHANNEL ( "SourceStreamChannel" )
#define IP_ADDRESS ( "IPAddress" )
#define GEV_CURRENT_IP_ADDRESS ( "GevCurrentIPAddress" )


#ifdef _AFXDLL
    IMPLEMENT_DYNAMIC( DeviceGEV, Device )
#endif // _AFXDLL


///
/// \brief Constructor
///

DeviceGEV::DeviceGEV( IMessageSink *aSink, LogBuffer *aLogBuffer )
    : Device( aSink, aLogBuffer )
{
    SetDevice( new PvDeviceGEV );
}


///
/// \brief Destructor
///

DeviceGEV::~DeviceGEV()
{

}


///
/// \brief Connects the device
///

PvResult DeviceGEV::Connect( IProgress *aProgress, Setup *aSetup, const PvDeviceInfo *aDeviceInfo, uint16_t &aChannel, const PvString &aCommunicationParameters )
{
    assert( aProgress != NULL );
    assert( aSetup != NULL );
    assert( aDeviceInfo != NULL );

    LoadCommunicationParameters( aCommunicationParameters );

    PvDeviceGEV *lDevice = dynamic_cast<PvDeviceGEV *>( GetDevice() );
    assert( lDevice != NULL );

    if ( aSetup->IsController() )
    {
        if ( !IsConnected() && aDeviceInfo != NULL )
        {
            SETSTATUS( "Connecting GigE Vision device..." );

            PvResult lResult = lDevice->Connect( aDeviceInfo );
            if ( !lResult.IsOK() )
            {
                return lResult;
            }
        }

        if ( aSetup->IsDataReceiver() )
        {
            // Determine whether this device is multi-source (does that GenICam feature exist)
            if ( IsMultiSourceTransmitter() )
            {
                // If multi-source feature exists, get the channel corresponding to the currently selected source
                // Keep in mind there's also the SourceComponentSelector feature that can be configured using the device parameter browser
                int64_t lValue = 0;
                PvResult lResult = GetParameters()->GetIntegerValue( SOURCE_STREAM_CHANNEL, lValue );
                if ( lResult.IsOK() )
                {
                    aChannel = static_cast<uint16_t>( lValue );
                }
                else
                {
                    aProgress->ShowWarning( "SourceSelector feature present, but SourceStreamChannel feature missing. Defaulting to channel 0." );
                }
            }

            const PvDeviceInfoGEV *lDeviceInfo = dynamic_cast<const PvDeviceInfoGEV *>( aDeviceInfo );
            assert( lDeviceInfo != NULL );

            // Negotiate packet size once for all channels
            if ( ( aDeviceInfo != NULL ) && ( lDeviceInfo->GetClass() == PvDeviceClassTransmitter ) )
            {
                NegotiatePacketSize( aProgress );
            }
        }
    }

    return PvResult::Code::OK;
}


///
/// \brief Sets the stream destination based on a specific application setup
///

PvResult DeviceGEV::SetStreamDestination( Setup *aSetup, Stream *aStream, uint16_t aChannel )
{
    if ( ( aSetup->GetRole() == Setup::RoleCtrlData ) ||
         ( aSetup->GetRole() == Setup::RoleCtrl ) )
    {
        if ( ( aSetup->GetDestination() == Setup::DestinationUnicastAuto ) ||
             ( aSetup->GetDestination() == Setup::DestinationUnicastSpecific ) )
        {
            assert( aSetup != NULL );
            assert( aStream != NULL );

            // If we have a stream, it is GEV. However, we may not have a stream (controller only)
            StreamGEV *lStreamGEV = dynamic_cast<StreamGEV *>( aStream );

            return SetStreamDestination( 
                lStreamGEV->GetLocalIPAddress().c_str(), 
                lStreamGEV->GetLocalPort(), 
                aChannel );
        }
        else if ( aSetup->GetDestination() == Setup::DestinationUnicastOther )
        {
            return SetStreamDestination( 
                aSetup->GetUnicastIP().GetAscii(), 
                aSetup->GetUnicastPort(), 
                aChannel );
        }
        else if ( aSetup->GetDestination() == Setup::DestinationMulticast )
        {
            return SetStreamDestination( 
                aSetup->GetMulticastIP().GetAscii(), 
                aSetup->GetMulticastPort(), 
                aChannel );
        }
    }

    return PvResult::Code::OK;
}


///
/// \brief Sets the stream destination of the device
///

PvResult DeviceGEV::SetStreamDestination( const std::string &aIpAddress, uint16_t aPort, uint16_t aChannel )
{
    PvDeviceGEV *lDevice = dynamic_cast<PvDeviceGEV *>( GetDevice() );
    assert( lDevice != NULL );

    // Only set destination if different than 0.0.0.0:0
    if ( ( aIpAddress == "0.0.0.0" ) &&
         ( aPort == 0 ) )
    {
        return PvResult::Code::OK;
    }

    return lDevice->SetStreamDestination( aIpAddress.c_str(), aPort, aChannel );
}


///
/// \brief Negotiates the packet size
///

PvResult DeviceGEV::NegotiatePacketSize( IProgress *aProgress )
{
    assert( aProgress != NULL );
    
    SETSTATUS( "Negotiating packet size..." );

    PvDeviceGEV *lDevice = dynamic_cast<PvDeviceGEV *>( GetDevice() );
    assert( lDevice != NULL );

    bool lEnabledValue = true;
    GetCommunicationParameters()->GetBooleanValue( AUTO_NEGOTIATION, lEnabledValue ) ;

    int64_t lUserPacketSizeValue = 1476;
    GetCommunicationParameters()->GetIntegerValue( DEFAULT_PACKET_SIZE, lUserPacketSizeValue );

    if ( lEnabledValue )
    {
        int64_t lPacketSizeValue = 0;
        GetParameters()->GetIntegerValue( GEV_SCPS_PACKET_SIZE, lPacketSizeValue );

        // Perform automatic packet size negotiation
        aProgress->SetStatus( "Optimizing streaming packet size..." );
        PvResult lResult = lDevice->NegotiatePacketSize( 0, static_cast<uint32_t>( lPacketSizeValue ) );
        if ( !lResult.IsOK() )
        {
            std::stringstream lSS;
            lSS << "WARNING: Streaming packet size negotiation failure, using original value of ";
            lSS << lPacketSizeValue << " bytes!";
        
            aProgress->ShowWarning( lSS.str() );
        }
    }
    else
    {
        PvResult lResult = lDevice->SetPacketSize( static_cast<uint32_t>( lUserPacketSizeValue ) );
        if ( !lResult.IsOK() )
        {
            // Last resort...
            lDevice->SetPacketSize( 576 );
        }

        std::stringstream lSS;
        if ( lResult.IsOK() )
        {
            lSS << "A packet of size of " << lUserPacketSizeValue;
            lSS << " bytes was configured for streaming. You may experience issues ";
            lSS << "if your system configuration cannot support this packet size.";
        }
        else
        {   
            lSS << "WARNING: could not set streaming packet size to " << lUserPacketSizeValue;
            lSS << " bytes, using 576 bytes!";
        }

        aProgress->ShowWarning( lSS.str() );
    }

    //
    // If more than one channel, propagate packet in effect size to other channels
    //

    int64_t lChannelCount = 0;
    PvResult lResult = GetParameters()->GetIntegerValue( GEV_STREAM_CHANNEL_COUNT, lChannelCount );
    if ( !lResult.IsOK() || ( lChannelCount < 2 ) )
    {
        // If we can't read the GevStreamChannelCount or of its less than 2, we are done here
        return PvResult::Code::OK;
    }

    int64_t lPacketSizeValue = 0;
    lResult = GetParameters()->GetIntegerValue( GEV_SCPS_PACKET_SIZE, lPacketSizeValue );
    if ( !lResult.IsOK() )
    {
        aProgress->ShowWarning( 
            "WARNING: Failed to propagate GevSCPSPacketSize on all streaming channels. "
            "Could not read back streaming channel 0 packet size. " );

        return lResult;
    }

    for ( int64_t i = 1; i < lChannelCount; i++ )
    {
        lResult = lDevice->SetPacketSize( static_cast<uint32_t>( lPacketSizeValue ), static_cast<uint32_t>( i ) );
        if ( !lResult.IsOK() )
        {
            aProgress->ShowWarning( "WARNING: Failed to propagate GevSCPSPacketSize on all streaming channels. " );
            return lResult;
        }
    }

    return PvResult::Code::OK;
}


///
/// \brief Return the local IP address (host)
///

std::string DeviceGEV::GetLocalIPAddress()
{
    PvGenInteger *lIPAddress = GetCommunicationParameters()->GetInteger( IP_ADDRESS );

    int64_t lValue = 0;
    lIPAddress->GetValue( lValue );

    std::stringstream lSS;
    lSS << ( ( lValue & 0xFF000000 ) >> 24 ) << ".";
    lSS << ( ( lValue & 0x00FF0000 ) >> 16 ) << ".";
    lSS << ( ( lValue & 0x0000FF00 ) >> 8 ) << ".";
    lSS << ( ( lValue & 0x000000FF ) );

    return lSS.str();
}


///
/// \brief Return the IP address of the device
///

std::string DeviceGEV::GetIPAddress()
{
    if ( !IsConnected() )
    {
        return "0.0.0.0";
    }

    PvGenInteger *lDeviceIPAddress = GetParameters()->GetInteger( GEV_CURRENT_IP_ADDRESS );

    int64_t lValue = 0;
    lDeviceIPAddress->GetValue( lValue );

    std::stringstream lSS;
    lSS << ( ( lValue & 0xFF000000 ) >> 24 ) << ".";
    lSS << ( ( lValue & 0x00FF0000 ) >> 16 ) << ".";
    lSS << ( ( lValue & 0x0000FF00 ) >> 8 ) << ".";
    lSS << ( ( lValue & 0x000000FF ) );

    return lSS.str();
}


///
/// \brief Resets the current stream destination
///

PvResult DeviceGEV::ResetStreamDestination( uint16_t aChannel )
{
    if ( !IsConnected()  )
    {
        return PvResult::Code::NOT_CONNECTED;
    }

    PvDeviceGEV *lDevice = dynamic_cast<PvDeviceGEV *>( GetDevice() );
    assert( lDevice != NULL );

    return lDevice->ResetStreamDestination( aChannel );
}


