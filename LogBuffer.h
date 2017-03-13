// *****************************************************************************
//
//     Copyright (c) 2010, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#ifndef __LOG_BUFFER_H__
#define __LOG_BUFFER_H__

#include <IPersistent.h>
#include <ParameterInfo.h>
#include <Mutex.h>

#include <PvConfigurationReader.h>
#include <PvConfigurationWriter.h>
#include <PvGenParameterArray.h>

#include <list>
#include <string>
#include <map>

#ifdef _LINUX_
#include <QtCore/QTime>
#endif // _AFXDLL


class LogBuffer
{
#ifdef _AFXDLL
    DECLARE_DYNAMIC( LogBuffer )
#endif

public:

    LogBuffer();
    ~LogBuffer();

    void Log( const std::string &aItem );
    
    std::list<std::string> &Lock();
    void Unlock();

    void EnableGenICamMonitoring( bool aEnabled );

    void Reset();

    void SetParameters( PvGenParameterArray *aParameters );
    bool IsSerialComLogSupported();

    void SetGenICamEnabled( bool aEnabled ) { mGenICamEnabled = aEnabled; }
    void SetEventsEnabled( bool aEnabled ) { mEventsEnabled = aEnabled; }
    void SetBufferErrorEnabled( bool aEnabled ) { mBufferErrorEnabled = aEnabled; }
    void SetBufferAllEnabled( bool aEnabled ) { mBufferAllEnabled = aEnabled; }
    void SetFilename( const PvString &aFilename );
    void SetWriteToFileEnabled( bool aEnabled );
    void SetSerialComLogEnabled( bool aEnabled );

    bool IsGenICamEnabled() const { return mGenICamEnabled; }
    bool IsEventsEnabled() const { return mEventsEnabled; }
    bool IsBufferErrorEnabled() const { return mBufferErrorEnabled; }
    bool IsBufferAllEnabled() const { return mBufferAllEnabled; }
    PvString GetFilename() const { return mFilename.c_str(); }
    bool IsWriteToFileEnabled() const { return mWriteToFileEnabled; }
    bool IsSerialComLogEnabled() const { return mSerialComLogEnabled; }

    // Persistence
    PvResult Save( PvConfigurationWriter *aWriter );
    PvResult Load( PvConfigurationReader *aReader );

    void CamHeadSerialComLog( const unsigned char* aData, uint32_t aDataLength, uint64_t aTimestamp );

    static std::string Unpack( const char *aPattern, const unsigned char *aData, uint32_t aDataLength, int aGap );

protected:

    void ResetConfig();
    uint32_t FindNext( const unsigned char *aData, uint32_t aDataLength, const unsigned char *aPattern, uint32_t aPatternLength );

private:

    Mutex mMutex;

#ifdef QT_VERSION
    QTime mTimer;
#else
    uint64_t mStartTimestamp;
#endif 

    std::list<std::string> mBuffer;

    bool mGenICamEnabled;
    bool mEventsEnabled;
    bool mBufferErrorEnabled;
    bool mBufferAllEnabled;
    bool mSerialComLogEnabled;

    std::string mFilename;
    bool mWriteToFileEnabled;

    PvGenParameterArray *mParameters;
    std::map<std::string, ParameterInfo> mInfoMap;

};


#endif // __LOG_BUFFER_H__
