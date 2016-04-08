// fetch_frame_rgb2jpeg.cpp : 定义控制台应用程序的入口点。
//
#if 0
利用ffmpeg从视频中提取I 帧（B 帧，P 帧）
发布者：hao zhang，发布时间：2013年10月3日 上午1 : 26
在视频处理中可以直接使用ffmpeg指令重视频中（MPEG）中单独提取I帧（P帧，B帧）也可以。
指令如下：
ffmpeg - i input - vf "select=eq(pict_type,PICT_TYPE_I)" output #to select only I frames
该指令的详细信息可以在一下两个链接中了解：

http ://ffmpeg.org/trac/ffmpeg/wiki/FilteringGuide
http ://ffmpeg.org/ffmpeg-filters.html#select_002c-aselect

    第二个url会详细解释select的用法。
#endif

#ifdef _WIN32
#include "stdafx.h"
#endif

extern "C"
{
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"
}

#ifdef _WIN32
#define snprintf _snprintf
#else
#define snprintf snprintf
#endif

static int save_frame_jpeg(const AVFrame *pFrameYUV, const char *pImgFName, int width, int height);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("usage: %s vide_infile\n", argv[0]);
        return -1;
    }
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket packet;
    AVFrame *pFrame, *pFrameYUV;
    SwsContext *pSWSCtx;
    enum AVPixelFormat dst_pix_fmt;

    const char *vide_outfile = argv[1], *img_outfile = argv[1];
    const char *codec_type, *codec_name;
    char szImgFName[128];
    int i, videoStream, pic_buf_size, gotFrame;
    int src_width = 0, src_height = 0;
    //int dst_width = 1920, dst_height = 1080;
    int dst_width = 320, dst_height = 240;
    //int dst_width = 1024, dst_height = 768;
    int64_t duration;
    uint8_t *pic_buf;

    int ret;

    av_register_all();

#if 0
    if (NULL == (pFormatCtx = avformat_alloc_context()))
    {
        printf("error!\n");
        return -1;
    }
#endif

    pFormatCtx = NULL;
    if ((ret = avformat_open_input(&pFormatCtx, vide_outfile, NULL, NULL)) < 0)
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }

    av_log(NULL, AV_LOG_INFO, "format: %s (%s)\n", pFormatCtx->iformat->name,
        pFormatCtx->iformat->long_name);

    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0)
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }

    duration = 0;
    if (pFormatCtx->duration != AV_NOPTS_VALUE)
    {
        duration = pFormatCtx->duration + 5000;
        duration /= AV_TIME_BASE; // sec
    }
    av_log(NULL, AV_LOG_INFO, "duration: %lld, ", (long long int)duration);

    // av_dump_format(pFormatCtx, 0, vide_outfile, false);

    videoStream = -1;

    for (i = 0; i < (int)pFormatCtx->nb_streams; i++)
    {
        pCodecCtx = pFormatCtx->streams[i]->codec;
        if (pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
        }
        if (AV_CODEC_ID_H264 != pCodecCtx->codec_id		///< video: h264
            && AV_CODEC_ID_MP3 != pCodecCtx->codec_id	///< audio: mp3
            && AV_CODEC_ID_AAC != pCodecCtx->codec_id)	///< audio: aac
        {
            return -1;
        }

        codec_type = av_get_media_type_string(pCodecCtx->codec_type);
        codec_name = avcodec_get_name(pCodecCtx->codec_id);
        av_log(NULL, AV_LOG_INFO, "%s: %s, ", codec_type, codec_name);
        if (pCodecCtx->codec && strcmp(pCodecCtx->codec->name, codec_name))
        {
            av_log(NULL, AV_LOG_INFO, " (%s)", pCodecCtx->codec->name);
        }
    }
    if (videoStream == -1)
    {
        printf("error! Didn't find a video stream\n");
        return -1;
    }

    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    src_width = pCodecCtx->width; src_height = pCodecCtx->height;
    av_log(NULL, AV_LOG_INFO, "video size: %dx%d", src_width, src_height);
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        printf("error!\n");
        return -1;
    }

    if ((ret = avcodec_open2(pCodecCtx, pCodec, NULL)) < 0)
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    //dst_width = 1920; dst_height = 1080;
    dst_width = 320; dst_height = 240;
    //dst_width = 1024; dst_height = 768;
    dst_pix_fmt = AV_PIX_FMT_YUV420P;
    pic_buf_size = av_image_get_buffer_size(dst_pix_fmt, dst_width, dst_height, 16);
    pic_buf = new uint8_t[pic_buf_size];
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, pic_buf,
        dst_pix_fmt, dst_width, dst_height, 16);

    pFrameYUV->width = dst_width;
    pFrameYUV->height = dst_height;
    pFrameYUV->format = dst_pix_fmt;

    pSWSCtx = sws_getContext(src_width, src_height,
        pCodecCtx->pix_fmt, dst_width, dst_height, dst_pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL);

    i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        if (packet.stream_index == videoStream)
        {
            if ((ret = avcodec_decode_video2(pCodecCtx, pFrame, &gotFrame, &packet) < 0))
            {
                printf("error: %s\n", av_err2str(ret));
                continue;
            }

            if (gotFrame)
            {
                if (pFrame->key_frame == 1)//这里取到关键帧数据
                {
                    // Warning: data is not aligned! This can lead to a speedloss
                    sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, src_height,
                        pFrameYUV->data, pFrameYUV->linesize);

                    pFrameYUV->pts = pFrame->pts; // av_frame_get_best_effort_timestamp(pFrameYUV);
                    snprintf(szImgFName, sizeof(szImgFName), "%s_%08d.jpg", img_outfile, i);
                    if (0 != save_frame_jpeg(pFrameYUV, szImgFName, dst_width, dst_height))
                    {
                        printf("error!\n");
                        return -1;
                    }

                    i++;
                }
            }
        }
        av_packet_unref(&packet);
    }
    printf("\nkey-frame numbers: %d", i);

    av_free(pFrameYUV);
    av_free(pFrame);
    sws_freeContext(pSWSCtx);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    system("pause");

    return 0;
}

static int save_frame_jpeg(const AVFrame *pFrameYUV, const char *pImgFName, int width, int height)
{
    AVFormatContext *pPicFmtCtx = NULL;
    AVOutputFormat *pPicOutFmt = NULL;
    AVStream *pPicVideoStrm = NULL;
    AVCodecContext *pPicCodecCtx = NULL;
    AVCodec *pPicCodec = NULL;
    AVPacket picPkt;
    int nGotPic, ret;

    if (NULL == (pPicFmtCtx = avformat_alloc_context()))
    {
        printf("error!\n");
        return -1;
    }
    if (NULL == (pPicOutFmt = av_guess_format("mjpeg", NULL, NULL)))
    {
        printf("error!\n");
        return -1;
    }
    pPicFmtCtx->oformat = pPicOutFmt;
    if ((ret = avio_open2(&pPicFmtCtx->pb, pImgFName, AVIO_FLAG_READ_WRITE,
        NULL, NULL) < 0))
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }
    if (NULL == (pPicVideoStrm = avformat_new_stream(pPicFmtCtx, NULL)))
    {
        printf("error!\n");
        return -1;
    }
    pPicVideoStrm->time_base.num = 1;
    pPicVideoStrm->time_base.den = 25;

    pPicCodecCtx = pPicVideoStrm->codec;
    pPicCodecCtx->codec_id = pPicOutFmt->video_codec;
    pPicCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pPicCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    pPicCodecCtx->width = width;
    pPicCodecCtx->height = height;
    pPicCodecCtx->time_base.num = 1;
    pPicCodecCtx->time_base.den = 25;

    av_dump_format(pPicFmtCtx, 0, pImgFName, 1);

    if (NULL == (pPicCodec = avcodec_find_encoder(pPicCodecCtx->codec_id)))
    {
        printf("error!\n");
        return -1;
    }
    if ((ret = avcodec_open2(pPicCodecCtx, pPicCodec, NULL) < 0))
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }

    avformat_write_header(pPicFmtCtx, NULL);

    av_new_packet(&picPkt, pPicCodecCtx->width * pPicCodecCtx->height * 3);
    if ((ret = avcodec_encode_video2(pPicCodecCtx, &picPkt, pFrameYUV, &nGotPic) < 0))
    {
        printf("error: %s\n", av_err2str(ret));
        return -1;
    }
    if (1 == nGotPic)
    {
        picPkt.stream_index = pPicVideoStrm->index;
        // [mjpeg @ 010bb200] Encoder did not produce proper pts, making some up.
        av_write_frame(pPicFmtCtx, &picPkt);
    }
    av_packet_unref(&picPkt);
    av_write_trailer(pPicFmtCtx);
    if (NULL != pPicVideoStrm)
    {
        avcodec_close(pPicVideoStrm->codec);
    }
    avio_close(pPicFmtCtx->pb);
    avformat_free_context(pPicFmtCtx);

    return 0;
}
