// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "eBUSPlayerShared.h"
#include "StreamU3V.h"

#include <PvStreamU3V.h>
#include <PvDeviceInfoU3V.h>
#include <PvUSBHostController.h>


#ifdef _AFXDLL
    IMPLEMENT_DYNAMIC( StreamU3V, Stream )
#endif // _AFXDLL


///
/// \brief Constructor
///

StreamU3V::StreamU3V( IMessageSink *aSink )
    : Stream( aSink )
    , mChannel( 0 )
{
    SetStream( new PvStreamU3V );
    SetPipeline( new PvPipeline( GetStream() ) );
}


///
/// \brief Destructor
///

StreamU3V::~StreamU3V()
{

}


///
/// \brief Opens a U3V stream object
///

PvResult StreamU3V::Open( Setup *aSetup, const PvDeviceInfo *aDeviceInfo, uint16_t aChannel ) 
{ 
    const PvDeviceInfoU3V *lDeviceInfo = dynamic_cast<const PvDeviceInfoU3V *>( aDeviceInfo );
    if ( lDeviceInfo == NULL )
    {
        return PvResult::Code::INVALID_PARAMETER;
    }

    PvStreamU3V *lStream = dynamic_cast<PvStreamU3V *>( GetStream() );
    if ( lDeviceInfo == NULL )
    {
        return PvResult::Code::INVALID_PARAMETER;
    }


    mDeviceGUID = lDeviceInfo->GetDeviceGUID().GetAscii();
    mChannel = aChannel;

    return lStream->Open( mDeviceGUID.c_str(), mChannel );
}


///
/// \brief Recovers a U3V stream. Simply closes and re-opens.
///

PvResult StreamU3V::Recover()
{
    PvStreamU3V *lStream = dynamic_cast<PvStreamU3V *>( GetStream() );
    PvResult lResult = lStream->Open( mDeviceGUID.c_str(), mChannel );

    return lResult;
}

