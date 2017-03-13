//
//  Mp4WriterMac.h
//  eBUSPlayer
//
//  Copyright (c) 2015 Pleora Technologies. All rights reserved.
//
#ifndef __MP4_WRITER_MAC_H__
#define __MP4_WRITER_MAC_H__

#include "IMp4Writer.h"
#include "Mutex.h"

struct MacMp4WriteWrapper;

class Mp4WriterMac : public IMp4Writer
{
public:
    
    Mp4WriterMac();
    virtual ~Mp4WriterMac();
    
    bool IsAvailable() const
    {
#ifdef PV_ENABLE_MP4
        return true;
#else
        return false;
#endif
    }
    bool IsOpened() { return ( mMacMp4Write != NULL ); }
    uint32_t GetAvgBitrate() const { return mAvgBitrate; }
    void SetAvgBitrate( uint32_t aValue ) { mAvgBitrate = aValue; }
    
    bool Open( const std::string &aFilename, PvImage *aImage );
    void Close();
    
    bool WriteFrame( PvImage *aImage, uint32_t *aFileSizeDelta = NULL );
    
    void GetLastError( PvString &aString ) { aString = ""; }
    void ResetLastError() {}
	
private:
    
    MacMp4WriteWrapper  *mMacMp4Write;
    Mutex                mMutex;
    
    uint64_t             mStartTm;
    uint32_t             mVideoTimestamp;
    uint32_t             mAvgBitrate;
    int                  mWidth;
    int                  mHeight;
    
};

#endif  //__MP4_WRITER_MAC_H__


