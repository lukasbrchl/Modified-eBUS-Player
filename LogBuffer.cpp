// *****************************************************************************
//
//     Copyright (c) 2010, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#include "eBUSPlayerShared.h"
#include "LogBuffer.h"

#ifdef WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

#include <assert.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>


#define LOGBUFFER_VERSION ( "1.0.0.0" )

#define TAG_VERSION ( "logbufferversion" )
#define TAG_GENICAMENABLED ( "loggenicamenabled" )
#define TAG_EVENTSENABLED ( "logeventsenabled" )
#define TAG_BUFFERALLENABLED ( "logbufferallenabled" )
#define TAG_BUFFERERRORENABLED ( "logbuffererrorenabled" )
#define TAG_LOGFILENAME ( "logfilename" )
#define TAG_WRITETOFILEENABLED ( "logwritetofileenabled" )
#define TAG_SERIALCOMLOGENABLED ( "logserialcomenabled" )

#define VAL_TRUE ( "true" )
#define VAL_FALSE ( "false" )


#define MAX_LOG_SIZE ( 1024 )
#define TERMINAL_COLUMNS ( 8 )


#ifdef _AFXDLL
    IMPLEMENT_DYNAMIC( LogBuffer, CObject )
#endif // _AFXDLL


// =============================================================================
LogBuffer::LogBuffer()
    : mGenICamEnabled( true )
    , mEventsEnabled( false )
    , mBufferAllEnabled( false )
    , mBufferErrorEnabled( true )
    , mWriteToFileEnabled( false )
    , mSerialComLogEnabled( false )
    , mParameters( NULL )
{
    ResetConfig();
    Reset();
}


// =============================================================================
LogBuffer::~LogBuffer()
{
}


// =============================================================================
void LogBuffer::ResetConfig()
{
    mGenICamEnabled = true;
    mEventsEnabled = false;
    mBufferAllEnabled = false;
    mBufferErrorEnabled = true;
    mWriteToFileEnabled = false;
    mSerialComLogEnabled = false;

#ifdef WIN32

    char lDesktop[ MAX_PATH ];
    SHGetSpecialFolderPathA( NULL, lDesktop, CSIDL_DESKTOP, true );

    mFilename = lDesktop;
    mFilename += "\\EventMonitor.txt";

#else

    struct passwd *pw = getpwuid( getuid() );
    
    mFilename = pw->pw_dir;
    mFilename += "/Desktop/EventMonitor.txt";

#endif //_LINUX_

}


// =============================================================================
void LogBuffer::Reset()
{
    mMutex.Lock();
    //////////////////////////////////

#ifdef QT_VERSION
    mTimer.start();
#else
    mStartTimestamp = ::GetTickCount();
#endif

    mBuffer.clear();

    //////////////////////////////////
    mMutex.Unlock();
}


// =============================================================================
void LogBuffer::Log( const std::string &aItem )
{
#ifdef _AFXDLL
#endif

#ifdef QT_VERSION
    int lElapsed = mTimer.elapsed();
#else
    int64_t lElapsed = ::GetTickCount() - mStartTimestamp;
#endif

    std::stringstream lStr;
    if ( lElapsed >= 0 )
    {
        lStr << std::setfill( '0' ) << std::setw( 4 ) << ( lElapsed / 1000 );
        lStr << ".";
        lStr << std::setfill( '0' ) << std::setw( 3 ) << ( lElapsed % 1000 );
    }
    else
    {
        lStr << "0";
    }

    lStr << "    " << aItem;

    mMutex.Lock();
    //////////////////////////////////

    mBuffer.push_back( lStr.str() );
    while ( mBuffer.size() > MAX_LOG_SIZE )
    {
        mBuffer.pop_front();
    }

    if ( mWriteToFileEnabled )
    {
        std::ofstream lFile;
        lFile.open( mFilename.c_str(), std::ios_base::app );
        if ( lFile.is_open() )
        {
            lFile << lStr.str() << "\n";
            lFile.close();
        }
    }

    //////////////////////////////////
    mMutex.Unlock();
}


// =============================================================================
std::list<std::string> &LogBuffer::Lock()
{
    mMutex.Lock();
    //////////////////////////////////

    return mBuffer;
}


// =============================================================================
void LogBuffer::Unlock()
{
	mBuffer.clear();

    //////////////////////////////////
    mMutex.Unlock();
}


// =============================================================================
void LogBuffer::SetFilename( const PvString &aFilename ) 
{ 
    mMutex.Lock();
    //////////////////////////////////

    mFilename = aFilename.GetAscii(); 

    //////////////////////////////////
    mMutex.Unlock();
}


// =============================================================================
void LogBuffer::SetWriteToFileEnabled( bool aEnabled ) 
{ 
    mMutex.Lock();
    //////////////////////////////////

    mWriteToFileEnabled = aEnabled; 

    //////////////////////////////////
    mMutex.Unlock();
}


// =============================================================================
PvResult LogBuffer::Save( PvConfigurationWriter *aWriter )
{
    // Save a version string, just in case we need it in the future
    aWriter->Store( LOGBUFFER_VERSION, TAG_VERSION );

    // bool mGenICamEnabled;
    aWriter->Store( mGenICamEnabled ? VAL_TRUE : VAL_FALSE, TAG_GENICAMENABLED );

    // bool mEventsEnabled;
    aWriter->Store( mEventsEnabled ? VAL_TRUE : VAL_FALSE, TAG_EVENTSENABLED );

    // bool mBufferAllEnabled;
    aWriter->Store( mBufferAllEnabled ? VAL_TRUE : VAL_FALSE, TAG_BUFFERALLENABLED );

    // bool mBufferErrorEnabled;
    aWriter->Store( mBufferErrorEnabled ? VAL_TRUE : VAL_FALSE, TAG_BUFFERERRORENABLED );

    // CString mLogFilename
    aWriter->Store( mFilename.c_str(), TAG_LOGFILENAME );

    // bool mWriteToFileEnabled;
    aWriter->Store( mWriteToFileEnabled ? VAL_TRUE : VAL_FALSE, TAG_WRITETOFILEENABLED );

    // bool mSerialComLogEnabled
    aWriter->Store( mSerialComLogEnabled ? VAL_TRUE : VAL_FALSE, TAG_SERIALCOMLOGENABLED );

    return PvResult::Code::OK;
}


// =============================================================================
PvResult LogBuffer::Load( PvConfigurationReader *aReader )
{
    PvResult lResult;
    PvString lPvStr;

    // Always load from a blank setup!
    ResetConfig();

    // bool mGenICamEnabled;
    lResult = aReader->Restore( TAG_GENICAMENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        mGenICamEnabled = ( lPvStr == VAL_TRUE );
    }

    // bool mEventsEnabled;
    lResult = aReader->Restore( TAG_EVENTSENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        mEventsEnabled = ( lPvStr == VAL_TRUE );
    }

    // bool mBufferAllEnabled;
    lResult = aReader->Restore( TAG_BUFFERALLENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        mBufferAllEnabled = ( lPvStr == VAL_TRUE );
    }

    // bool mBufferErrorEnabled;
    lResult = aReader->Restore( TAG_BUFFERERRORENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        mBufferErrorEnabled = ( lPvStr == VAL_TRUE );
    }

    // CString mFilename;
    lResult = aReader->Restore( TAG_LOGFILENAME, lPvStr );
    if ( lResult.IsOK() )
    {
        SetFilename( lPvStr.GetUnicode() );
    }

    // bool mWriteToFileEnabled;
    lResult = aReader->Restore( TAG_WRITETOFILEENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        SetWriteToFileEnabled( lPvStr == VAL_TRUE );
    }

    // bool mSerialComLogEnabled
    lResult = aReader->Restore( TAG_SERIALCOMLOGENABLED, lPvStr );
    if ( lResult.IsOK() )
    {
        mSerialComLogEnabled = ( lPvStr == VAL_TRUE );
    }

    return PvResult::Code::OK;
}


// =============================================================================
bool LogBuffer::IsSerialComLogSupported()
{
    if ( mParameters == NULL )
    {
        return false;
    }

    // Get required parameters
    PvGenEnum *lEventSelector = mParameters->GetEnum( "EventSelector" );
    PvGenEnum *lEventNotification = mParameters->GetEnum( "EventNotification" );
    if ( ( lEventSelector == NULL ) || ( lEventNotification == NULL ) )
    {
        return false;
    }

    // Look for event selector specific enum entry
    int64_t lCount = 0;
    lEventSelector->GetEntriesCount( lCount );
    for ( int64_t i = 0; i < lCount; i++ )
    {
        const PvGenEnumEntry *lEE;
        lEventSelector->GetEntryByIndex( i, &lEE );

        PvString lName;
        lEE->GetName( lName );

        if ( lName == "IPEngineCamHeadSerialComLog" )
        {
            return true;
        }
    }

    return false;
}


// =============================================================================
void LogBuffer::SetSerialComLogEnabled( bool aEnabled )
{
    // Get required parameters
    PvGenEnum *lEventSelector = mParameters->GetEnum( "EventSelector" );
    PvGenEnum *lEventNotification = mParameters->GetEnum( "EventNotification" );
    if ( ( lEventSelector == NULL ) || ( lEventNotification == NULL ) )
    {
        mSerialComLogEnabled = false;
        return;
    }

    // Select IPEngineCamHeadSerialComLog
    PvResult lResult = lEventSelector->SetValue( "IPEngineCamHeadSerialComLog" );
    if ( !lResult.IsOK() )
    {
        mSerialComLogEnabled = false;
        return;
    }

    // Enable event
    lResult = lEventNotification->SetValue( aEnabled ? "GigEVisionEvent" : "Off" );
    if ( !lResult.IsOK() )
    {
        lResult = lEventNotification->SetValue( aEnabled ? "On" : "Off" );
        if ( !lResult.IsOK() )
        {
            mSerialComLogEnabled = false;
            return;
        }
    }

    mSerialComLogEnabled = true;
}


// =============================================================================
void LogBuffer::CamHeadSerialComLog( const unsigned char* aData, uint32_t aDataLength, uint64_t aTimestamp )
{
    const char *cPatterns[] = { "Tx: ", "Rx: " };

    std::stringstream lBuffer;
    lBuffer << "video interface <-> camera head\r\nTimestamp: ";
    lBuffer << std::uppercase << std::hex << std::setfill( '0' ) << std::setw( 16 ) << aTimestamp;
    lBuffer << "\r\n";

    uint32_t lStart = 0;
    for ( int i = 0; ; i++ )
    {
        // Alternate between patterns
        const char *lPattern = cPatterns[ i % 2 ];
        size_t lLength = strlen( lPattern );

        // Find Tx/Rx
        uint32_t lIndex = FindNext( aData + lStart, aDataLength - lStart, 
            reinterpret_cast<const unsigned char *>( lPattern ), static_cast<uint32_t>( lLength ) );

        // Set end to end of data or new found tag
        uint32_t lEnd = ( lIndex <= 0 ) ? aDataLength : ( lIndex + lStart ) - 1;

        // If we have a start, unpack
        if ( ( lStart > 0 ) && ( lEnd > lStart ) )
        {
            // Unpack Rx
            const char *lPattern = cPatterns[ ( i + 1 ) % 2 ]; // Compensate, we're always late by one
            lBuffer << Unpack( lPattern, aData + lStart, lEnd - lStart, 1 );
        }

        // If nothing found, end of data
        if ( lIndex <= 0 )
        {
            break;
        }

        // Update start before going for another round
        lStart = ( lIndex + lStart ) + static_cast<uint32_t>( lLength );
    }

    Log( lBuffer.str().c_str() );
}


// =============================================================================
uint32_t LogBuffer::FindNext( const unsigned char *aData, uint32_t aDataLength, 
    const unsigned char *aPattern, uint32_t aPatternLength )
{
    uint32_t j = 0;
    const unsigned char *lData = aData;
    for ( uint32_t i = 0; i < aDataLength; i++ )
    {
        if ( lData[ i ] == aPattern[ j ] )
        {
            j++;
            if ( j == aPatternLength )
            {
                return ( i - j + 1 );
            }
        }
        else
        {
            j = 0;
        }
    }

    return 0;
}


// =============================================================================
std::string LogBuffer::Unpack( const char *aPattern, const unsigned char *aData, uint32_t aDataLength, int aGap )
{
    std::stringstream lBuffer;

    size_t lLength = strlen( aPattern );
    for ( size_t j = 0; j < lLength; j++ )
    {
        lBuffer << aPattern[ j ];
    }

    std::vector<unsigned char> lLine;
    uint32_t i = 0;
    for ( ;; ) // Skip NULLs
    {
        unsigned char lChar = aData[ i ];

        // Format, append hex value
        lBuffer << std::uppercase << std::hex << std::setfill( '0' ) << std::setw( 2 ) << static_cast<uint32_t>( lChar ) << ' ';

        // 0x20: chars below or equal to space, cannot be displayed
        // 0x7F: delete
        // 0xAF: extended ASCII, cannot be displayed
        if ( ( lChar <= 0x20 ) || ( lChar > 0x7F ) || ( lChar == 127 ) )
        {
            lChar = '.';
        }

        // Save character for latter
        lLine.push_back( lChar );

        // Next character, skipping possible inter-char
        i += 1 + aGap;

        // If end of terminal line reached - or end of data output readable chars
        if ( ( ( ( i / ( 1 + aGap ) ) % TERMINAL_COLUMNS ) == 0 ) || ( i >= aDataLength ) )
        {
            int lMissing = TERMINAL_COLUMNS - ( ( ( i - ( 1 + aGap ) ) / ( 1 + aGap ) ) % TERMINAL_COLUMNS );
            for ( int k = 0; k < lMissing + 1; k++ )
            {
                lBuffer << "   ";
            }
            
            // Flush characters buffer
            std::vector<unsigned char>::iterator lIt = lLine.begin();
            while ( lIt != lLine.end() )
            {
                lBuffer << *lIt;
                lIt++;
            }
            lLine.clear();

            // Still data? If so, start new line
            if ( i < aDataLength )
            {
                // New line
                lBuffer << "\r\n";

                // Align data
                for ( size_t k = 0; k < lLength; k++ )
                {
                    lBuffer << " ";
                }
            }
        }

        // Reached end of data?
        if ( i >= aDataLength )
        {
            break;
        }
    }

    lBuffer << "\r\n";

    return lBuffer.str();
}


// =============================================================================
void LogBuffer::SetParameters( PvGenParameterArray *aParameters )
{
    if ( aParameters == NULL )
    {
        mInfoMap.clear();
        mParameters = NULL;
        return;
    }

    assert( mInfoMap.size() <= 0 );
    assert( mParameters == NULL );

    mParameters = aParameters;
    for ( uint32_t i = 0; i < mParameters->GetCount(); i++ )
    {
        PvGenParameter *lParameter = mParameters->Get( i );

        // We only display invisible parameter events when in debug mode
#ifndef _DEBUG
        PvGenVisibility lVisibility = PvGenVisibilityUndefined;
        lParameter->GetVisibility( lVisibility );
        if ( lVisibility < PvGenVisibilityInvisible )
#endif // _DEBUG
        {
            PvString lName;
            lParameter->GetName( lName );

            mInfoMap[ lName.GetAscii() ].Initialize( lParameter, this );
        }
    }
}


// ==============================================================================
void LogBuffer::EnableGenICamMonitoring( bool aEnabled )
{
    std::map<std::string, ParameterInfo>::iterator lIt = mInfoMap.begin();
    while ( lIt != mInfoMap.end() )
    {
        lIt->second.Enable( aEnabled );
        lIt++;
    }
}


