/**
 * ============================================================================
 * @file	download_rtmp.cpp
 *
 * @brief   最简单的基于FFmpeg的收流器（接收RTMP）
 *			Simplest FFmpeg Receiver (Receive RTMP)
 *
 * @details 本例子将流媒体数据（以RTMP为例）保存成本地文件。
 *			是使用FFmpeg进行流媒体接收最简单的教程。
 * 
 *			This example saves streaming media data (Use RTMP 
 *			as example) as a local file.
 *			It's the simplest FFmpeg stream receiver.
 *
 * @version 1.0  
 * @date	2015/10/12 15:20
 *
 * @author  志杰, shizhijie@baofeng.com
 * ============================================================================
 */

#include "stdafx.h"

#define __STDC_CONSTANT_MACROS

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"

#ifdef __cplusplus
};
#endif

///< '1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 3)
	{
		printf("usage: %s rtmp_url ofname\n", argv[0]);
		getchar();

		return -1;
	}

	AVOutputFormat *p_ofmt = NULL;
	///< Input AVFormatContext and Output AVFormatContext
	AVFormatContext *p_ifmt_ctx = NULL, *p_ofmt_ctx = NULL;
	AVPacket pkt;
	const char *p_in_fname, *p_out_fname;
	int ret, i;
	int video_idx = -1, frame_idx = 0;

	p_in_fname = argv[1];
	p_out_fname = argv[2];

	av_register_all();
	///< Network
	avformat_network_init();
	///< Input
	if ((ret = avformat_open_input(&p_ifmt_ctx, p_in_fname, NULL, NULL)) < 0)
	{
		printf("Could not open input file.\n");
		goto end;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx, NULL)) < 0)
	{
		printf("Failed to retrieve input stream information.\n");
		goto end;
	}

	for (i = 0; i < (int)p_ifmt_ctx->nb_streams; ++i)
	{
		if (AVMEDIA_TYPE_VIDEO == p_ifmt_ctx->streams[i]->codec->codec_type)
		{
			video_idx = i;
			break;
		}
	}
	av_dump_format(p_ifmt_ctx, 0, p_in_fname, 0);

	///< Output
	avformat_alloc_output_context2(&p_ofmt_ctx, NULL, NULL, p_out_fname); ///< RTMP
	if (NULL == p_ofmt_ctx)
	{
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt = p_ofmt_ctx->oformat;
	for (i = 0; i < (int)p_ifmt_ctx->nb_streams; ++i)
	{
		///< Create output AVStream according to input AVStream
		AVStream *p_in_strm = p_ifmt_ctx->streams[i];
		AVStream *p_out_strm = avformat_new_stream(p_ofmt_ctx, p_in_strm->codec->codec);
		if (NULL == p_out_strm)
		{
			printf("Failed allocating output stream.\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		///< Copy the settings of AVCodecContext
		if ((ret = avcodec_copy_context(p_out_strm->codec, p_in_strm->codec)) < 0)
		{
			printf("Failed to copy context from input to output stream codec context.\n");
			goto end;
		}
		p_out_strm->codec->codec_tag = 0;
		if (p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			p_out_strm->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}
	///< Dump Format ----------------
	av_dump_format(p_ofmt_ctx, 0, p_out_fname, 1);
	///< Open output URL
	if (!(p_ofmt->flags & AVFMT_NOFILE))
	{
		if ((ret = avio_open(&p_ofmt_ctx->pb, p_out_fname, AVIO_FLAG_WRITE)) < 0)
		{
			printf("Could not open output URL '%s'.\n", p_out_fname);
			goto end;
		}
	}
	///< Write file header
	if ((ret = avformat_write_header(p_ofmt_ctx, NULL)) < 0)
	{
		printf("Error occurred when opening output URL.\n");
		goto end;
	}

#if USE_H264BSF
	AVBitStreamFilterContext *p_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

	while (true)
	{
		AVStream *p_in_strm, *p_out_strm;
		///< Get an AVPacket
		if ((ret = av_read_frame(p_ifmt_ctx, &pkt)) < 0)
		{
			break;
		}

		p_in_strm = p_ifmt_ctx->streams[pkt.stream_index];
		p_out_strm = p_ofmt_ctx->streams[pkt.stream_index];

		///< Copy packet
		///< Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, p_in_strm->time_base, p_out_strm->time_base,
			(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, p_in_strm->time_base, p_out_strm->time_base,
			(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = (int)av_rescale_q(pkt.duration, p_in_strm->time_base,
			p_out_strm->time_base);
		pkt.pos = -1;
		///< Print to Screen
		if (pkt.stream_index == video_idx)
		{
			printf("Receive %8d video frames from input URL.\n", frame_idx);
			++frame_idx;

#if USE_H264BSF
			av_bitstream_filter_filter(p_h264bsfc, p_in_strm->codec, NULL,
				&pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
		}

		///< ret = av_write_frame(p_ofmt_ctx, &pkt);
		if ((ret = av_interleaved_write_frame(p_ofmt_ctx, &pkt)) < 0)
		{
			printf("Error muxing packet.\n");
			break;
		}
		av_free_packet(&pkt);
	}

#if USE_H264BSF
	av_bitstream_filter_close(p_h264bsfc);
#endif

	///< Write file trailer
	av_write_trailer(p_ofmt_ctx);

end:
	avformat_close_input(&p_ifmt_ctx);
	///< Close output
	if (p_ofmt_ctx && !(p_ofmt->flags & AVFMT_NOFILE))
	{
		avio_close(p_ofmt_ctx->pb);
	}
	avformat_free_context(p_ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("Error occurred.\n");
		return -1;
	}

	return 0;
}
