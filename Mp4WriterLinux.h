// *****************************************************************************
//
//     Copyright (c) 2015, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

#ifndef __MP4_WRITER_LINUX_H__
#define __MP4_WRITER_LINUX_H__

#include "IMp4Writer.h"
#include "Mutex.h"

class SwsContext;
class AVCodec;
class AVCodecContext;
class AVFrame;
class AVFormatContext;

class Mp4WriterLinux : public IMp4Writer
{
public:

    Mp4WriterLinux();
    virtual ~Mp4WriterLinux();

    bool IsAvailable() const { return true; }
    bool IsOpened() { return mInitialized; }
    uint32_t GetAvgBitrate() const { return mAvgBitrate; }
    void SetAvgBitrate( uint32_t aValue ) { mAvgBitrate = aValue; }

    bool Open( const std::string &aFilename, PvImage *aImage );
    void Close();

    bool WriteFrame( PvImage *aImage, uint32_t *aFileSizeDelta = NULL );

    void GetLastError( PvString &aString ) { aString = ""; }
    void ResetLastError() {}

private:

     Mutex mMutex;

     int              mI420Stride[3];
     uint8_t         *mI420Data[3];
     SwsContext      *mI420Converter;
     AVCodec         *mCodecEncode;
     AVCodecContext  *mCtxEncode;
     AVFrame         *mPictureEncoded;
     AVFormatContext *mFormatContext;
     bool             mInitialized;
     uint64_t         mStartTm;
     uint32_t         mVideoTimestamp;
     uint32_t         mWidth;
     uint32_t         mHeight;
     uint32_t         mAvgBitrate;
     uint32_t         mRecordFps;
};


#endif // __MP4_WRITER_LINUX_H__

