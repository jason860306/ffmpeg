// fetch_frame.cpp : 定义控制台应用程序的入口点。
//
#if 0
利用ffmpeg从视频中提取I 帧（B 帧，P 帧）
发布者：hao zhang，发布时间：2013年10月3日 上午1 : 26
在视频处理中可以直接使用ffmpeg指令重视频中（MPEG）中单独提取I帧（P帧，B帧）也可以。
指令如下：
ffmpeg -i input -vf "select=eq(pict_type,PICT_TYPE_I)" output #to select only I frames
该指令的详细信息可以在一下两个链接中了解：

http://ffmpeg.org/trac/ffmpeg/wiki/FilteringGuide
http://ffmpeg.org/ffmpeg-filters.html#select_002c-aselect

第二个url会详细解释select的用法。
#endif

#include "stdafx.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2)
	{
		printf("usage: %s video_file\n", argv[0]);
		return -1;
	}
	SwsContext *pSWSCtx;
	AVFormatContext *pFormatCtx;
	const char *filename = argv[1];
	int i, videoStream;
	AVCodecContext *pCodecCtx;
	AVFrame *pFrame;
	AVFrame *pFrameRGB;
	int     numBytes, frameFinished;
	uint8_t *buffer;
	static AVPacket packet;

	av_register_all();

#if 0
	- if ((ret = av_open_input_file(&movie->format_ctx, movie->file_name, iformat, 0, NULL)) < 0) {
	+ if ((ret = avformat_open_input(&movie->format_ctx, movie->file_name, iformat, NULL)) < 0) {
#endif
	if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
	{
		printf("error!\n");
		return -1;
	}

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("error!\n");
		return -1;
	}

	av_dump_format(pFormatCtx, 0, filename, false);

	videoStream = -1;

	for (i = 0; i < (int)pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		videoStream = i;
		break;
	}
	if (videoStream == -1)
	{
		printf("error! Didn't find a video stream\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	AVCodec *pCodec;

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("error!\n");
		return -1;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("error!\n");
		return -1;
	}

	pFrame = av_frame_alloc();

	pFrameRGB = av_frame_alloc();
	numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height, 32);
	buffer = new uint8_t[numBytes];
#if 0
	int avpicture_fill(AVPicture *picture, const uint8_t *ptr,  
		enum AVPixelFormat pix_fmt, int width, int height)
	{
		return av_image_fill_arrays(picture->data, picture->linesize,
			ptr, pix_fmt, width, height, 1);
	}
#endif
		
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);
	pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		if (packet.stream_index == videoStream)
		{
#if 0
			int avcodec_decode_video(AVCodecContext* avctx, AVFrame* picture,
				int* got_picture_ptr, uint8_t* buf, int buf_size) {

				AVPacket pkt;
				av_init_packet(&pkt);
				pkt.data = buf;
				pkt.size = buf_size;
				return avcodec_decode_video2(avctx, picture, got_picture_ptr, &pkt);
			}
#endif
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (frameFinished)
			{
				if (pFrame->key_frame == 1)//这里取到关键帧数据   
				{
					sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
					i++;
				}
			}
		}
		av_packet_unref(&packet);
	}
	av_free(pFrameRGB);
	av_free(pFrame);
	sws_freeContext(pSWSCtx);
	avcodec_close(pCodecCtx);
#if 0
	- av_close_input_file(ic);
	+ avformat_close_input(&ic);
#endif
	avformat_close_input(&pFormatCtx);

	return 0;

}