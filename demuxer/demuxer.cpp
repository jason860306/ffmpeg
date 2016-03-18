/**
 * ============================================================================
 * @file	demuxer.cpp
 *
 * @brief
 * @details This software split a media file (in Container such as
 *			MKV, FLV, AVI...) to video and audio bitstream.
 *			In this example, it demux a MPEG2TS file to H.264
 *			bitstream and AAC bitstream.
 *
 * @version 1.0
 * @date	2015/10/08 14:51
 *
 * @author  Ö¾½Ü, shizhijie@baofeng.com
 * ============================================================================
 */

#include "stdafx.h"

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
extern "C"
{
#endif // __cplusplus

#include "libavformat/avformat.h"

#ifdef __cplusplus
};
#endif // __cplusplus

/**
 * @brief FIX: H.264 in some container format (FLV, MP4, MKV etc.)
 * need "h264_mp4toannexb" bitstream filter (BSF)
 *   *Add SPS,PPS in front of IDR frame
 *   *Add start code("0,0,0,1") in front of NALU
 * H.264 in some container (MPEG2TS) don't need this BSF.
 */

// '1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 4)
	{
		printf("Usage: %s in_fname out_video_fname out_audio_fname\n");
		return -1;
	}

	AVOutputFormat *p_ofmt_a = NULL, *p_ofmt_v = NULL;
	// (Input AVFormatContext and Output AVFormatContext)
	AVFormatContext *p_ifmt_ctx = NULL, *p_ofmt_ctx_a = NULL, *p_ofmt_ctx_v = NULL;
	AVPacket pkt;
	int ret, i, video_idx = -1, audio_idx = -1, frame_idx = 0;

	const char *p_in_fnamt = NULL, *p_out_fname_v = NULL, *p_out_fname_a = NULL;

	p_in_fnamt = argv[1]; ///< Input file URL
	p_out_fname_v = argv[2]; ///< Output file URL for video
	p_out_fname_a = argv[3]; ///< Output file URL for audio 

	av_register_all();

	///< Input
	if ((ret = avformat_open_input(&p_ifmt_ctx, p_in_fnamt, NULL, NULL)) < 0)
	{
		printf("Could not open input file(: %s).\n");
		goto end;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx, NULL)) < 0)
	{
		printf("Failed to retrieve input stream information.\n");
		goto end;
	}

	///< Output
	avformat_alloc_output_context2(&p_ofmt_ctx_v, NULL, NULL, p_out_fname_v);
	if (NULL == p_ofmt_ctx_v)
	{
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt_v = p_ofmt_ctx_v->oformat;

	avformat_alloc_output_context2(&p_ofmt_ctx_a, NULL, NULL, p_out_fname_a);
	if (NULL == p_ofmt_ctx_a)
	{
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt_a = p_ofmt_ctx_a->oformat;

	for (i = 0; i < (int)p_ifmt_ctx->nb_streams; ++i)
	{
		///< Create output AVStream according to input AVStream
		AVFormatContext *p_ofmt_ctx = NULL;
		AVStream *p_in_strm = p_ifmt_ctx->streams[i];
		AVStream *p_out_strm = NULL;

		if (AVMEDIA_TYPE_VIDEO == p_ifmt_ctx->streams[i]->codec->codec_type)
		{
			video_idx = i;
			p_out_strm = avformat_new_stream(p_ofmt_ctx_v, p_in_strm->codec->codec);
			p_ofmt_ctx = p_ofmt_ctx_v;
		}
		else if (AVMEDIA_TYPE_AUDIO == p_ifmt_ctx->streams[i]->codec->codec_type)
		{
			audio_idx = i;
			p_out_strm = avformat_new_stream(p_ofmt_ctx_a, p_in_strm->codec->codec);
			p_ofmt_ctx = p_ofmt_ctx_a;
		}
		else
		{
			break;
		}

		if (NULL == p_out_strm)
		{
			printf("Failed allocating output stream.\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		///< Copy the settings of AVCodecContext
		if (avcodec_copy_context(p_out_strm->codec, p_in_strm->codec) < 0)
		{
			printf("Failed to copy context frame input to output stream codec context.\n");
			goto end;
		}
		p_out_strm->codec->codec_tag = 0;
		if (p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			p_out_strm->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	///< Dump Format------------------------------
	printf("\n=================Input Video=================\n");
	av_dump_format(p_ifmt_ctx, 0, p_in_fnamt, 0);
	printf("\n=================Output Video=================\n");
	av_dump_format(p_ofmt_ctx_v, 0, p_out_fname_v, 1);
	printf("\n=================Output Audio=================\n");
	av_dump_format(p_ofmt_ctx_a, 0, p_out_fname_a, 1);
	printf("\n==================================\n");

	///< Open output file
	if (!(p_ofmt_v->flags & AVFMT_NOFILE))
	{
		if (avio_open(&p_ofmt_ctx_v->pb, p_out_fname_v, AVIO_FLAG_WRITE) < 0)
		{
			printf("Could not open output file '%s'", p_out_fname_v);
			goto end;
		}
	}

	if (!(p_ofmt_a->flags & AVFMT_NOFILE))
	{
		if (avio_open(&p_ofmt_ctx_a->pb, p_out_fname_a, AVIO_FLAG_WRITE) < 0)
		{
			printf("Could not open output file '%s'", p_out_fname_a);
			goto end;
		}
	}

	///< Write file header
	if (avformat_write_header(p_ofmt_ctx_v, NULL) < 0)
	{
		printf("Error occurred when opening video output file\n");
		goto end;
	}
	if (avformat_write_header(p_ofmt_ctx_a, NULL) < 0)
	{
		printf("Error occurred when opening audio output file\n");
		goto end;
	}

#if USE_H264BSF
	AVBitStreamFilterContext *p_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

	while (true)
	{
		AVFormatContext *p_ofmt_ctx;
		AVStream *p_in_strm, *p_out_strm;

		///< Get an AVPacket
		if (av_read_frame(p_ifmt_ctx, &pkt) < 0)
		{
			break;
		}
		p_in_strm = p_ifmt_ctx->streams[pkt.stream_index];
		if (pkt.stream_index == video_idx)
		{
			p_out_strm = p_ofmt_ctx_v->streams[0];
			p_ofmt_ctx = p_ofmt_ctx_v;
			printf("Write Video Packet. size: %d\tpts: %lld\n",
				pkt.size, pkt.pts);
#if USE_H264BSF
			av_bitstream_filter_filter(p_h264bsfc, p_in_strm->codec, NULL, &pkt.data,
				&pkt.size, pkt.data, pkt.size, 0);
#endif
		}
		else if (pkt.stream_index == audio_idx)
		{
			p_out_strm = p_ofmt_ctx_a->streams[0];
			p_ofmt_ctx = p_ofmt_ctx_a;
			printf("Write Audio Packet. size: %d\tpts: %lld\n",
				pkt.size, pkt.pts);
		}
		else
		{
			continue;
		}

		///< Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = (int)av_rescale_q(pkt.duration, p_in_strm->time_base,
			p_out_strm->time_base);
		pkt.pos = -1;
		pkt.stream_index = 0;

		///< Write
		if (av_interleaved_write_frame(p_ofmt_ctx, &pkt) < 0)
		{
			printf("Error muxing packet\n");
			break;
		}

		//printf("Write %8d frames to output file\n", frame_idx);
		av_free_packet(&pkt);
		++frame_idx;
	}

#if USE_H264BSF
	av_bitstream_filter_close(p_h264bsfc);
#endif

	///< Write file trailer
	av_write_trailer(p_ofmt_ctx_a);
	av_write_trailer(p_ofmt_ctx_v);

end:
	avformat_close_input(&p_ifmt_ctx);
	///< close output
	if (p_ofmt_ctx_a && !(p_ofmt_a->flags & AVFMT_NOFILE))
	{
		avio_close(p_ofmt_ctx_a->pb);
	}
	if (p_ofmt_ctx_v && !(p_ofmt_v->flags & AVFMT_NOFILE))
	{
		avio_close(p_ofmt_ctx_v->pb);
	}

	avformat_free_context(p_ofmt_ctx_a);
	avformat_free_context(p_ofmt_ctx_v);
	p_ofmt_ctx_a = NULL;
	p_ofmt_ctx_v = NULL;

	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("Error occurred.\n");
		return -1;
	}

	return 0;
}
