/**
 * ============================================================================
 * @file	muxer.cpp
 *
 * @brief   最简单的基于FFmpeg的视音频复用器 
 * @details 本程序可以将视频码流和音频码流打包到一种封装格式中。
 *			程序中将AAC编码的音频码流和H.264编码的视频码流打包成
 *			MPEG2TS封装格式的文件。
 *			需要注意的是本程序并不改变视音频的编码格式。
 *			
 *			This software mux a video bitstream and a audio bitstream
 *			together into a file. In this example, it mux a H.264
 *			bitstream (in MPEG2TS) and a AAC bitstream file together
 *			into MP4 format file.
 *
 * @version 1.0  
 * @date	2015/10/09 15:49
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

#ifdef __cplusplus
};
#endif

/**
 * @brief FIX: H.264 in some container format (FLV, MP4, MKV etc.)
 *		  need "h264_mp4toannexb" bitstream filter (BSF)
 *		  * Add SPS,PPS in front of IDR frame
 *		  * Add start code ("0,0,0,1") in front of NALU
 *		  H.264 in some container (MPEG2TS) don't need this BSF.
 */

///< '1': Use H.264 Bitstream Filter
#define USE_H264BSF 1

/**
 * @brief FIX: AAC in some container format (FLV, MP4, MKV etc.) need
 *		  "aac_adtstoasc" bitstream filter (BSF)
 */

///< 'a': Use AAC Bitstream Filter
#define USE_AACBSF 1

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 4)
	{
		printf("Usage: %s in_fname_v in_fname_a out_fname\n");
		return -1;
	}
	AVOutputFormat *p_ofmt = NULL;
	///< Input AVFormatContext and Output AVFormatContext
	AVFormatContext *p_ifmt_ctx_v = NULL, *p_ifmt_ctx_a = NULL, *p_ofmt_ctx = NULL;
	AVPacket pkt;

	int ret, i;
	int video_idx_v = -1, video_idx_out = -1;
	int audio_idx_a = -1, audio_idx_out = -1;
	int frame_idx = 0;
	int64_t cur_pts_v = 0, cur_pts_a = 0;

	const char *p_in_fname_v = argv[1], *p_in_fname_a = argv[2], *p_out_fname = argv[3];

	av_register_all();

	///< Input
	if ((ret = avformat_open_input(&p_ifmt_ctx_v, p_in_fname_v, NULL, NULL)) < 0)
	{
		printf("Could not open input file(: %s).\n", p_in_fname_v);
		goto end;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx_v, NULL)) < 0)
	{
		printf("Failed to retrieve input stream information.\n");
		goto end;
	}

	if ((ret = avformat_open_input(&p_ifmt_ctx_a, p_in_fname_a, NULL, NULL)) < 0)
	{
		printf("Could not open input file.\n");
		goto end;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx_a, NULL)) < 0)
	{
		printf("Failed to retrieve input stream information.\n");
		goto end;
	}
	printf("=========Input Information=========\n");
	av_dump_format(p_ifmt_ctx_v, 0, p_in_fname_v, 0);
	av_dump_format(p_ifmt_ctx_a, 0, p_in_fname_a, 0);
	printf("===================================\n");

	///< Output
	avformat_alloc_output_context2(&p_ofmt_ctx, NULL, NULL, p_out_fname);
	if (NULL == p_ofmt_ctx)
	{
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt = p_ofmt_ctx->oformat;

	for (i = 0; i < (int)p_ifmt_ctx_v->nb_streams; ++i)
	{
		///< Create output AVStream according to input AVStream
		if (AVMEDIA_TYPE_VIDEO == p_ifmt_ctx_v->streams[i]->codec->codec_type)
		{
			AVStream *p_in_strm = p_ifmt_ctx_v->streams[i];
			AVStream *p_out_strm = avformat_new_stream(p_ofmt_ctx,
				p_in_strm->codec->codec);
			video_idx_v = i;
			if (NULL == p_out_strm)
			{
				printf("Failed allocating output stream.\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			video_idx_out = p_out_strm->index;

			///< Copy the settings of AVCodecContext
			if (avcodec_copy_context(p_out_strm->codec, p_in_strm->codec) < 0)
			{
				printf("Failed to copy context from input to output"
					" stream codec context.\n");
				goto end;
			}
			p_out_strm->codec->codec_tag = 0;
			if (p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			{
				p_out_strm->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
			break;
		}
	}

	for (i = 0; i < (int)p_ifmt_ctx_a->nb_streams; ++i)
	{
		///< Create output AVStream according to input AVStream
		if (AVMEDIA_TYPE_AUDIO == p_ifmt_ctx_a->streams[i]->codec->codec_type)
		{
			AVStream *p_in_strm = p_ifmt_ctx_a->streams[i];
			AVStream *p_out_strm = avformat_new_stream(p_ofmt_ctx,
				p_in_strm->codec->codec);
			audio_idx_a = i;
			if (NULL == p_out_strm)
			{
				printf("Failed allocating output stream.\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			audio_idx_out = p_out_strm->index;

			///< Copy the settings of AVCodecContext
			if (avcodec_copy_context(p_out_strm->codec, p_in_strm->codec) < 0)
			{
				printf("Failed to copy context from intput to "
					"output stream codec context.\n");
				goto end;
			}
			p_out_strm->codec->codec_tag = 0;
			if (p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			{
				p_out_strm->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
			break;
		}
	}

	printf("=========Output Information=========\n");
	av_dump_format(p_ofmt_ctx, 0, p_out_fname, 1);
	printf("====================================\n");

	///< Open output file
	if (!(p_ofmt->flags & AVFMT_NOFILE))
	{
		if (avio_open(&p_ofmt_ctx->pb, p_out_fname, AVIO_FLAG_WRITE) < 0)
		{
			printf("Could not open output file '%s'", p_out_fname);
			goto end;
		}
	}
	///< Write file header
	if ((ret = avformat_write_header(p_ofmt_ctx, NULL)) < 0)
	{
		printf("Error occurred when opening output file.\n");
		goto end;
	}

	///< FIX
#if USE_H264BSF
	AVBitStreamFilterContext *p_h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif

#if USE_AACBSF
	AVBitStreamFilterContext *p_aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif

	while (true)
	{
		AVFormatContext *p_ifmt_ctx;
		int strm_idx = 0;
		AVStream *p_in_strm, *p_out_strm;

		///< Get an AVPacket
		if (av_compare_ts(cur_pts_v, p_ifmt_ctx_v->streams[video_idx_v]->time_base,
			cur_pts_a, p_ifmt_ctx_a->streams[audio_idx_a]->time_base) <= 0)
		{
			p_ifmt_ctx = p_ifmt_ctx_v;
			strm_idx = video_idx_out;

			if (av_read_frame(p_ifmt_ctx, &pkt) >= 0)
			{
				do 
				{
					p_in_strm = p_ifmt_ctx->streams[pkt.stream_index];
					p_out_strm = p_ofmt_ctx->streams[strm_idx];

					if (pkt.stream_index == video_idx_v)
					{
						///< FIX: No PTS (Example: Raw H.264)
						///< Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE)
						{
							///< Write PTS
							AVRational time_base1 = p_in_strm->time_base;
							///< Duration between 2 frames (us)
							int64_t calc_duration = (int64_t)((double)AV_TIME_BASE /
								av_q2d(p_in_strm->r_frame_rate));
							///< Parameters
							pkt.pts = (int64_t)((double)(frame_idx * calc_duration) /
								(double)(av_q2d(time_base1) * AV_TIME_BASE));
							pkt.dts = pkt.pts;
							pkt.duration = (int)((double)calc_duration /
								(double)(av_q2d(time_base1) * AV_TIME_BASE));
							++frame_idx;
						}
						cur_pts_v = pkt.pts;
						break;
					}
				} while (av_read_frame(p_ifmt_ctx, &pkt));
			}
			else
			{
				break;
			}
		}
		else
		{
			p_ifmt_ctx = p_ifmt_ctx_a;
			strm_idx = audio_idx_out;
			if (av_read_frame(p_ifmt_ctx, &pkt) >= 0)
			{
				do 
				{
					p_in_strm = p_ifmt_ctx->streams[pkt.stream_index];
					p_out_strm = p_ofmt_ctx->streams[strm_idx];

					if (pkt.stream_index == audio_idx_a)
					{
						///< FIX: No PTS
						///< Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE)
						{
							///< Write PTS
							AVRational time_base1 = p_in_strm->time_base;
							///< Duration between 2 frames (us)
							int64_t calc_duration = (int64_t)((double)AV_TIME_BASE /
								av_q2d(p_in_strm->r_frame_rate));
							///< Parameters
							pkt.dts = (int64_t)((double)(frame_idx * calc_duration) /
								(double)(av_q2d(time_base1) * AV_TIME_BASE));
							pkt.dts = pkt.pts;
							pkt.duration = (int)((double)calc_duration /
								(double)(av_q2d(time_base1)* AV_TIME_BASE));
							++frame_idx;
						}
						cur_pts_a = pkt.pts;
						break;
					}
				} while (av_read_frame(p_ifmt_ctx, &pkt));
			}
			else
			{
				break;
			}
		}

		///< FIX: Bitstream Filter
#if USE_H264BSF
		av_bitstream_filter_filter(p_h264bsfc, p_in_strm->codec, NULL,
			&pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

#if USE_AACBSF
		av_bitstream_filter_filter(p_aacbsfc, p_out_strm->codec, NULL,
			&pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

		///< Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = (int)av_rescale_q(pkt.duration, p_in_strm->time_base, p_out_strm->time_base);
		pkt.pos = -1;
		pkt.stream_index = strm_idx;

		printf("Write 1 Packet. size: %5d\tpts: %11d\n", pkt.size, pkt.pts);
		///< Write
		if (av_interleaved_write_frame(p_ofmt_ctx, &pkt) < 0)
		{
			printf("Error muxing packet.\n");
			break;
		}
		av_free_packet(&pkt);
	}

	///< Write file trailer
	av_write_trailer(p_ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(p_h264bsfc);
#endif

#if USE_AACBSF
	av_bitstream_filter_close(p_aacbsfc);
#endif

end:
	avformat_close_input(&p_ifmt_ctx_v);
	avformat_close_input(&p_ifmt_ctx_a);

	///< close output
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
