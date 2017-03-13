//
//     Copyright (c) 2015, Pleora Technologies Inc., All rights reserved.
//
// mp4 file recording (of just video) using libav involves doing these:
// -setting up the codec (h264 or mpeg4) and the parameters (bitrate, size, threads, quality, etc)
// -getting 'extra data' from the codec that is to be included in mp4 file header in avcc format
//   in case of h264 this is the SPS/PPS written in a particular format (called avcc)
//   the codec does not give the extra data in avcc format, libav converts the SPS/PPS to avcc
// -setting up the format context with the appropriate codec for mp4 file saving
// -opening the mp4 file
// -encoding incoming I420 images into h264/mpeg4 and writing the results into the file
//    taking note to set timestamps appropriate relative to the set fps
//    fps of the file is set once but the timestamp values can vary the playing time
// -when the file is finished (taking care of flushing the encoder), a trailer must be written
//    that contains moov atom and other atoms in mp4 that are tables for seeking and finding frames
// -it is also important to know that while the encoder may be writing the frames in "network format"
// or annexb format (h264), they must be written with avc format into the file. This is currently handled
// automatically by libav however
// *****************************************************************************

#include "eBUSPlayerShared.h"
#include "Mp4WriterLinux.h"

#include <PvSampleUtils.h>
#include <PvString.h>
#include <PvImage.h>
#include <iostream>

#ifdef PV_ENABLE_MP4
extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/opt.h"
    #include "libswscale/swscale.h"
};

const uint32_t DEFAULT_VIDEO_BIT_RATE = 2000000;
const uint32_t VIDEO_FPS = 10;
const PvPixelType DST_PIXEL_TYPE = PvPixelBGRa8;

///
/// \brief Constructor
///

Mp4WriterLinux::Mp4WriterLinux()
    :mI420Converter( NULL )
    ,mCodecEncode( NULL )
    ,mCtxEncode( NULL )
    ,mPictureEncoded( NULL )
    ,mFormatContext( NULL )
    ,mInitialized( false )
    ,mStartTm( 0 )
    ,mVideoTimestamp( 0 )
    ,mWidth( 0 )
    ,mHeight( 0 )
    ,mAvgBitrate( DEFAULT_VIDEO_BIT_RATE )
    ,mRecordFps( VIDEO_FPS )
{
    //these two functions need to be called once per process and are not thread safe
    //if different threads create their own Mp4Writer instance, it may cause an issue
    av_log_set_level( AV_LOG_ERROR );
    av_register_all();
    mI420Data[ 0 ] = NULL;
    mI420Data[ 1 ] = NULL;
    mI420Data[ 2 ] = NULL;
    mI420Stride[ 0 ] = 0;
    mI420Stride[ 1 ] = 0;
    mI420Stride[ 2 ] = 0;
}


///
/// \brief Destructor
///

Mp4WriterLinux::~Mp4WriterLinux()
{
    Close();
}


///
/// \brief Open the MP4, prepares for receiving frames
///

bool Mp4WriterLinux::Open( const std::string &aFilename, PvImage *aImage )
{
    if ( !IsAvailable() )
    {
        return false;
    }
    if ( mInitialized || mWidth || mHeight )
    {
        //an already open encoder should be closed first
        return false;
    }

    MutexHolder m( &mMutex );
    //////////////////////////////////

    mWidth = aImage->GetWidth();
    mHeight = aImage->GetHeight();

    //YUV420planer output data
    mI420Data[ 0 ]= (uint8_t*) malloc( mWidth * mHeight );   //Y plane
    mI420Data[ 1 ]= (uint8_t*) malloc( mWidth * mHeight / 4 ); //U plane
    mI420Data[ 2 ]= (uint8_t*) malloc( mWidth * mHeight / 4 ); //V plane
    mI420Stride[ 0 ] = mWidth;
    mI420Stride[ 1 ] = mWidth / 2;
    mI420Stride[ 2 ] = mWidth / 2;

    //use swscale for BGRa (32bit) to YUV420 planar conversion
    //assumption is that incoming format will always be BGRa
    mI420Converter = sws_getContext(
            mWidth, mHeight, PIX_FMT_BGR32,
            mWidth, mHeight, PIX_FMT_YUV420P,
            SWS_FAST_BILINEAR, NULL, NULL, NULL );
    if ( mI420Converter == NULL )
    {
        return false;
    }

    //todo: experiment and allow for mpeg4 (CODEC_ID_MPEG4) recording
    AVCodecID lCodecId = AV_CODEC_ID_H264;
    mCodecEncode = avcodec_find_encoder( lCodecId );
    if ( mCodecEncode == NULL )
    {
        return false;
    }
    mCtxEncode = avcodec_alloc_context3( mCodecEncode );
    if ( mCtxEncode != NULL )
    {
        //These are encoding parameters and determine the speed, quality and bitrate of the produced video
        //the encoder will encode assuming frame rate of 30fps. If incoming fps is different, then
        //bitrate will be different (eg if incoming fps is 10fps, then bitrate will be mAvgBitrate/3 instead of mAvgBitrate)
        mCtxEncode->width = mWidth;
        mCtxEncode->height = mHeight;
        mCtxEncode->time_base.num = 1;
        mCtxEncode->time_base.den = mRecordFps;
        mCtxEncode->pix_fmt = PIX_FMT_YUV420P;
        mCtxEncode->max_b_frames = 0;
        mCtxEncode->has_b_frames = 0;
        //todo: bitrate should be adapted to the size of image
        mCtxEncode->bit_rate = (int)( mAvgBitrate * 0.80f );
        mCtxEncode->bit_rate_tolerance = (int) ( mAvgBitrate * 0.20f );
        //It is important to set this flag so that headers (SPS and PPS for h264) are written into
        //the extra bytes on open and not for every IFrame
        mCtxEncode->flags |= CODEC_FLAG_GLOBAL_HEADER;
        //one Iframe per 30 frames (or once a second if at 30fps)
        //if incoming fps is different than 30fps, IFrame period will vary
        mCtxEncode->gop_size = mRecordFps * 3;
        mCtxEncode->keyint_min = mRecordFps;
    }
    else
    {
        return false;
    }
    //If CODEC_FLAG_GLOBAL_HEADER flag was set in open, the headers (SPS PPS of h264) are written
    //into the extra data of mCtxEncode which is needed to be written as header of the mp4 file
    // open codec for h264 encoder
    if ( avcodec_open2( mCtxEncode, mCodecEncode, NULL ) < 0 )
    {
        return false;
    }
    // alloc image and output buffer for encoder
    mPictureEncoded = avcodec_alloc_frame();
    if ( mPictureEncoded == NULL )
    {
        return false;
    }
    AVOutputFormat *lOutputFormat = NULL;
    // Create container
    lOutputFormat = av_guess_format( 0, aFilename.c_str(), 0 );
    if ( lOutputFormat == NULL )
    {
        return false;
    }
    mFormatContext = avformat_alloc_context();
    if ( mFormatContext == NULL )
    {
       return false;
    }
    mFormatContext->oformat = lOutputFormat;
    strcpy( mFormatContext->filename, aFilename.c_str() );

    // Add video stream
    AVStream *pst = avformat_new_stream( mFormatContext, NULL );
    if ( pst != NULL && pst->codec != NULL )
    {
        pst->codec->codec_type = AVMEDIA_TYPE_VIDEO;
        pst->codec->pix_fmt = AV_PIX_FMT_NV12;
        pst->codec->codec_id = lCodecId;
        pst->codec->bit_rate = mAvgBitrate;
        pst->codec->width = mWidth;
        pst->codec->height = mHeight;
        pst->codec->time_base.num = 1;
        pst->codec->time_base.den = mRecordFps;
        pst->codec->thread_count = 1;

        // setup extradata to be those written by codec into mCtxEncode
        // this should be SPS/PPS of h264
        pst->codec->extradata_size = mCtxEncode->extradata_size;
        pst->codec->extradata = mCtxEncode->extradata;

        if(mFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        {
            pst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        if ( !( mFormatContext->oformat->flags & AVFMT_NOFILE ) )
        {
            if ( avio_open( &mFormatContext->pb, mFormatContext->filename, AVIO_FLAG_WRITE ) )
            {
                return false;
            }
        }
        //this is where the mp4 headers (avcc header is written based on extradata)
        avformat_write_header( mFormatContext , NULL );
        mInitialized = true;
    }
    else
    {
       return false;
    }
    return true;
}


///
/// \brief Closes the MP4
///

void Mp4WriterLinux::Close()
{
    if ( !IsAvailable() )
    {
        return;
    }

    MutexHolder m(&mMutex);
    //////////////////////////////////
    if ( mCtxEncode != NULL )
    {
        //flush the encoder for any additional frames
        for (int lGotOutput = 1; lGotOutput; )
        {
            AVPacket lAvpkt;
            av_init_packet( &lAvpkt );
            lAvpkt.size = 0;
            lAvpkt.data = NULL;
            fflush(stdout);
            if ( avcodec_encode_video2( mCtxEncode, &lAvpkt, NULL, &lGotOutput ) < 0 )
            {
                //this is an error condition. should stop encode
                lGotOutput = 0;
            }
            if ( lGotOutput )
            {
                if ( mFormatContext != NULL )
                {
                    lAvpkt.pts = lAvpkt.dts = mVideoTimestamp++;
                    int lResult2 = av_write_frame( mFormatContext, &lAvpkt );
                }
                av_free_packet( &lAvpkt );
            }
        }
    }
    if ( mFormatContext != NULL )
    {
        //must write the trailer for moov atom to be written
        av_write_trailer( mFormatContext );

        if ( mFormatContext->oformat && !( mFormatContext->oformat->flags & AVFMT_NOFILE ) && mFormatContext->pb )
        {
            avio_close( mFormatContext->pb );
        }
        if ( mFormatContext->streams )
        {
            if (mFormatContext->streams[0]->codec)
            {
                //extradata was part of mCtxEncode and will be freed when mCtxEncode is freed
                mFormatContext->streams[0]->codec->extradata = NULL;
                mFormatContext->streams[0]->codec->extradata_size = 0;
            }
        }
        avformat_free_context( mFormatContext );
        mFormatContext = NULL;
    }
    mCodecEncode = NULL;
    if ( mCtxEncode != NULL )
    {
        avcodec_close( mCtxEncode );
        av_free( mCtxEncode );
        mCtxEncode = NULL;
    }
    if ( mPictureEncoded != NULL)
    {
        avcodec_free_frame( &mPictureEncoded );
        mPictureEncoded = NULL;
    }
    if ( mI420Converter != NULL )
    {
        sws_freeContext( mI420Converter );
        mI420Converter = NULL;
    }
    if ( mI420Data[ 0 ] != NULL )
    {
        free( mI420Data[ 0 ] );
        mI420Data[ 0 ] = NULL;
    }
    if ( mI420Data[ 1 ] != NULL )
    {
        free( mI420Data[ 1 ] );
        mI420Data[ 1 ] = NULL;
    }
    if ( mI420Data[ 2 ] != NULL )
    {
        free( mI420Data[ 2 ] );
        mI420Data[ 2 ] = NULL;
    }
    mI420Stride[ 0 ] = 0;
    mI420Stride[ 1 ] = 0;
    mI420Stride[ 2 ] = 0;
    mInitialized = false;
    mWidth = 0;
    mHeight = 0;
    mVideoTimestamp = 0;
    mStartTm = 0;
    //////////////////////////////////
}

///
/// \brief Writes a PvImage to the MP4/H.264 container
///

bool Mp4WriterLinux::WriteFrame( PvImage *aImage, uint32_t *aFileSizeDelta )
{
    MutexHolder m(&mMutex);
    if ( !IsAvailable() || !mInitialized )
    {
        return false;
    }

    if ( aImage->GetWidth() != mWidth || aImage->GetHeight() != mHeight )
    {
        //can not change size of encode midstream
        return false;
    }

    if ( aImage->GetPixelType() != DST_PIXEL_TYPE)
    {
        //currently only support BGRa input
        return false;
    }


    //convert to YUV420 planar
    int pic_size = mWidth * mHeight;
    avcodec_get_frame_defaults( mPictureEncoded );
    mPictureEncoded->format = PIX_FMT_YUV420P;
    mPictureEncoded->width = mWidth;
    mPictureEncoded->height = mHeight;
    mPictureEncoded->linesize[ 0 ] = mWidth;
    mPictureEncoded->linesize[ 1 ] = mWidth / 2;
    mPictureEncoded->linesize[ 2 ] = mWidth / 2;
    mPictureEncoded->data[ 0 ] = mI420Data[ 0 ];
    mPictureEncoded->data[ 1 ] = mI420Data[ 1 ];
    mPictureEncoded->data[ 2 ] = mI420Data[ 2 ];

    uint8_t *lData[ AV_NUM_DATA_POINTERS ] = { 0 };
    lData[ 0 ] = aImage->GetDataPointer();

    int lLinesize[ AV_NUM_DATA_POINTERS ] = { 0 };
    lLinesize[ 0 ] = mWidth * 4;

    sws_scale( mI420Converter, lData, lLinesize, 0, mHeight, mI420Data, mI420Stride );

    //timestamps in libav are based on the time_scale set in FormatContext (ie mRecordFps).
    //for example for 10fps, timescale is 10. So frames coming at 0, 100ms, 200ms (perfect 10ms)
    //will have timestamps 0, 1, 2,
    //so the millisecond timestamp is calculated as = (timestamp * 1000)/timescale
    //and so the timestamp is calculated as = (time diff * timescale)/1000
    //note that time diff should be calculated from a start time and not just relative
    //to last frame to avoid long term rounding off problem
    uint64_t lNow = PvGetTickCountMs();
    if ( mStartTm )
    {
        if ( lNow > mStartTm )
        {
            mVideoTimestamp = (( lNow - mStartTm ) * mRecordFps ) / 1000;
        }
        else
        {
            ++mVideoTimestamp;
        }
    }
    else
    {
        mStartTm = lNow;
    }

    mPictureEncoded->pts = mVideoTimestamp;

    //avpkt will get allocated by the encode function
    //it should be freed only if "lGotPacketPtr" is set
    AVPacket lAvpkt;
    av_init_packet( &lAvpkt );
    lAvpkt.size = 0;
    lAvpkt.data = NULL;
    int lGotPacketPtr = 0;

    // encode frame
    int lEncResult = avcodec_encode_video2( mCtxEncode, &lAvpkt, mPictureEncoded, &lGotPacketPtr );
    if( lEncResult < 0 )
    {
        return false;
    }
    else if ( lGotPacketPtr == 0 )
    {
        lEncResult = avcodec_encode_video2( mCtxEncode, &lAvpkt, NULL, &lGotPacketPtr );
    }
    if ( lAvpkt.size )
    {
        lAvpkt.stream_index = mFormatContext->streams[0]->index;

        if ( av_write_frame( mFormatContext, &lAvpkt ) )
        {
            av_free_packet(&lAvpkt);
            return false;
        }
        av_free_packet(&lAvpkt);
    }
    else
    {
        return false;
    }
    return true;
}

#endif



