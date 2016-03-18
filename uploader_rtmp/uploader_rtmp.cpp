/**
 * ============================================================================
 * @file	uploader_rtmp.cpp
 *
 * @brief   最简单的基于FFmpeg的推流器（推送RTMP）
 *			Simplest FFmpeg Streamer (Send RTMP)
 *			
 * @details 本例子实现了推送本地视频至流媒体服务器（以RTMP为例）。
 *			是使用FFmpeg进行流媒体推送最简单的教程。
 *
 * @version 1.0  
 * @date	2015/10/10 16:30
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

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		printf("usage: %s input rtmp_url\n", argv[0]);
		getchar();

		return -1;
	}
	AVOutputFormat *p_ofmt = NULL;

	///< 输入对应一个AVFormatContext，输出对应一个AVFormatContext
	///< (Input AVFormatContext and Output AVFormatContext)
	AVFormatContext *p_ifmt_ctx = NULL, *p_ofmt_ctx = NULL;
	AVPacket pkt;
	const char *p_in_fname = NULL, *p_out_fname = NULL;

	int ret, i;
	int video_idx = -1, frame_idx = 0;
	int64_t start_time = 0;

	p_in_fname = argv[1];
	p_out_fname = argv[2];
	///< "rtmp://localhost/publishlive/livestream" ///<输出 URL（Output URL）[RTMP] 
	///<out_filename = "rtp://233.233.233.233:6666"; ///< 输出 URL（Output URL）[UDP]

	av_register_all();
	///< Network
	avformat_network_init();

	///< 输入(Input)
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

	///< 输出(Output)
	avformat_alloc_output_context2(&p_ofmt_ctx, NULL, "flv", p_out_fname); ///< RTMP
	///< avformat_alloc_output_context2(&p_ofmt_ctx, NULL, "mpegts", p_out_fname); ///< UDP
	if (NULL == p_ofmt_ctx)
	{
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt = p_ofmt_ctx->oformat;
	for (i = 0; i < (int)p_ifmt_ctx->nb_streams; ++i)
	{
		///< 根据输入流创建输出流(Create output AVStream according to input AVStream)
		AVStream *p_in_strm = p_ifmt_ctx->streams[i];
		AVStream *p_out_strm = avformat_new_stream(p_ofmt_ctx, p_in_strm->codec->codec);
		if (NULL == p_out_strm)
		{
			printf("Failed allocating output stream.\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		///< 复制AVCodecContext的设置(Copy the settings of AVCodecContext)
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

	///< Dump Format----------------------------
	av_dump_format(p_ofmt_ctx, 0, p_out_fname, 1);
	///< 打开输出URL(Open output URL)
	if (!(p_ofmt->flags & AVFMT_NOFILE))
	{
		if ((ret = avio_open(&p_ofmt_ctx->pb, p_out_fname, AVIO_FLAG_WRITE)) < 0)
		{
			printf("Could not open output URL '%s'\n", p_out_fname);
			goto end;
		}
	}

	///< 写文件头(Write file header)
	if ((ret = avformat_write_header(p_ofmt_ctx, NULL)) < 0)
	{
		printf("Error occurred when opening output URL\n");
		goto end;
	}

	start_time = av_gettime();
	while (true)
	{
		AVStream *p_in_strm, *p_out_strm;
		///< 获取一个AVPacket(Get an AVPacket)
		if ((ret = av_read_frame(p_ifmt_ctx, &pkt)) < 0)
		{
			break;
		}

		///< FIX: No PTS (Example: Raw H.264)
		///< Simple Write PTS
		if (pkt.pts == AV_NOPTS_VALUE)
		{
			///< Write PTS
			AVRational time_base1 = p_ifmt_ctx->streams[video_idx]->time_base;
			///< Duration between 2 frames (us)
			int64_t calc_duration = (int64_t)((double)AV_TIME_BASE /
				av_q2d(p_ifmt_ctx->streams[video_idx]->r_frame_rate));
			///< Parameters
			pkt.pts = (int64_t)((double)(frame_idx * calc_duration) /
				(double)(av_q2d(time_base1) * AV_TIME_BASE));
			pkt.dts = pkt.pts;
			pkt.duration = (int)((double)calc_duration /
				(double)(av_q2d(time_base1) * AV_TIME_BASE));
		}

		///< Important: Delay
		if (pkt.stream_index == video_idx)
		{
			AVRational time_base = p_ifmt_ctx->streams[video_idx]->time_base;
			AVRational time_base_q = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
			{
				av_usleep((unsigned int)(pts_time - now_time));
			}
		}

		p_in_strm = p_ifmt_ctx->streams[pkt.stream_index];
		p_out_strm = p_ofmt_ctx->streams[pkt.stream_index];

		///< copy packet
		///< 转换PTS/DTS(Convert PTS/DTS)
		pkt.pts = av_rescale_q_rnd(pkt.pts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, p_in_strm->time_base,
			p_out_strm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = (int)av_rescale_q(pkt.duration, p_in_strm->time_base,
			p_out_strm->time_base);
		pkt.pos = -1;

		///< Print to Screen
		if (pkt.stream_index == video_idx)
		{
			printf("Send %8d video frames to output URL\n", frame_idx);
			++frame_idx;
		}

		if ((ret = av_interleaved_write_frame(p_ofmt_ctx, &pkt)) < 0)
		{
			printf("Error muxing packet.\n");
			break;
		}

		av_free_packet(&pkt);
	}

	///< 写文件尾(Write file trailer)
	av_write_trailer(p_ofmt_ctx);

end:
	avformat_close_input(&p_ifmt_ctx);
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
