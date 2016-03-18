// demuxer_simple.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <locale.h>

extern "C"
{
#include "libavformat/avformat.h"
};

#define USE_H264BSF 1

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 4)
	{
		printf("Usage: %s in_fname out_video_fname out_audio_fname\n");
		return -1;
	}

	AVFormatContext *p_ifmt_ctx = NULL;
	AVPacket pkt;

	int ret = 0;
	int video_idx = -1, audio_idx = -1;

	const char *p_in_fname = NULL, *p_out_vfname = NULL, *p_out_afname = NULL;
	p_in_fname = argv[1];
	p_out_vfname = argv[2];
	p_out_afname = argv[3];

	av_register_all();

	///< Input
	if ((ret = avformat_open_input(&p_ifmt_ctx, p_in_fname, NULL, NULL)) < 0)
	{
		printf("Could not open input file.");
		return -1;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx, NULL)) < 0)
	{
		printf("Failed to retrieve input stream information");
		return -1;
	}

	video_idx = -1;
	for (unsigned int i = 0; i < p_ifmt_ctx->nb_streams; ++i)
	{
		if (AVMEDIA_TYPE_VIDEO == p_ifmt_ctx->streams[i]->codec->codec_type)
		{
			video_idx = i;
		}
		else if (AVMEDIA_TYPE_AUDIO == p_ifmt_ctx->streams[i]->codec->codec_type)
		{
			audio_idx = i;
		}
	}

	///< Dump Format
	printf("\nInput Video======================================\n");
	av_dump_format(p_ifmt_ctx, 0, p_in_fname, 0);
	printf("\n=================================================\n");

	FILE *p_faudio = NULL, *p_fvideo = NULL;

	if (NULL == (p_faudio = fopen(p_out_afname, "wb+")))
	{
		printf("open audio file(: %s) failed!\n");
		return -1;
	}
	if (NULL == (p_fvideo = fopen(p_out_vfname, "wb+")))
	{
		printf("open video file(: %s) failed!\n");
		return -1;
	}

	/**
	 * @brief FIX: H.264 in some container format(flv, mp4, mkv etc.)
	 * need "h264_mp4toannexb" bitstream filter(BSF)
	 *	1. Add SPS, PPS in front of IDR frame
	 *	2. Add start code("0,0,0,1") in front of NALU
	 * H.264 in some container(MPEG2TS) don't need this BSF
	 */
#if USE_H264BSF
	AVBitStreamFilterContext *p_h264bsfc = av_bitstream_filter_init(
		"h264_mp4toannexb");
#endif

	while (av_read_frame(p_ifmt_ctx, &pkt) >= 0)
	{
		if (pkt.stream_index == video_idx)
		{
#if USE_H264BSF
			av_bitstream_filter_filter(p_h264bsfc, p_ifmt_ctx->streams[video_idx]->codec,
				NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
			printf("Write Video Packet. size: %d\t pts: %lld\n", pkt.size, pkt.pts);
			fwrite(pkt.data, 1, pkt.size, p_fvideo);
		}
		else if (pkt.stream_index == audio_idx)
		{
			/**
			 * @brief AAC in some container format(flv, mp4, mkv 
			 *		  etc.) need to add 7 Bytes ADTS Header in 
			 *		  front of AVPacket data manually. Other Audio
			 *		  Codec(mp3...) works well
			 */
			printf("Write Audio Packet. size: %d\t pts: %lld\n", pkt.size, pkt.pts);
			fwrite(pkt.data, 1, pkt.size, p_faudio);
		}
		av_free_packet(&pkt);
	}

#if USE_H264BSF
	av_bitstream_filter_close(p_h264bsfc);
#endif

	fclose(p_fvideo); p_fvideo = NULL;
	fclose(p_faudio); p_faudio = NULL;

	avformat_close_input(&p_ifmt_ctx);
	
	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("Error occurred.\n");
		return -1;
	}

	return 0;
}
