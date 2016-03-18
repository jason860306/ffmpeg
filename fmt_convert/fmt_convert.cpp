// fmt_convert.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

extern "C"
{
#include "libavformat/avformat.h"
}

int _tmain(int argc, _TCHAR* argv[])
{
	AVOutputFormat *p_ofmt = NULL;

	/**
	 * @brief 输入对应一个AVFormatContext，输出对应一个AVFormatContext
	 *		  （Input AVFormatContext and Output AVFormatContext）
	 */
	AVFormatContext *p_ifmt_ctx = NULL, *p_ofmt_ctx = NULL;
	AVPacket pkt;
	const char *p_in_fname = NULL, *p_out_fname = NULL;

	int ret = 0;
	if (argc < 3)
	{
		printf("usage: %s input output\n"
			"Remux a media file with libavformat and libavcodec.\n"
			"The output format is guessed according to the file extension.\n", argv[0]);
		getchar();

		return -1;
	}

	p_in_fname = argv[1]; ///< 输入文件名（Input file URL)
	p_out_fname = argv[2]; ///< 输出文件名（Output file URL)
	av_register_all();

	///< 输入（Input)
	if ((ret = avformat_open_input(&p_ifmt_ctx, p_in_fname, 0, 0)) < 0)
	{
		printf("Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(p_ifmt_ctx, 0)) < 0)
	{
		printf("Failed to retrieve input stream information.");
		goto end;
	}
	av_dump_format(p_ifmt_ctx, 0, p_in_fname, 0);

	///< 输出（Output）
	avformat_alloc_output_context2(&p_ofmt_ctx, NULL, NULL, p_out_fname);
	if (NULL == p_ofmt_ctx)
	{
		printf("Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	p_ofmt = p_ofmt_ctx->oformat;
	for (unsigned int i = 0; i < p_ifmt_ctx->nb_streams; ++i)
	{
		///< 根据输入流创建输出流（Create output AVStream according to input AVStream）
		AVStream *p_in_stream = NULL, *p_out_stream = NULL;
		p_in_stream = p_ifmt_ctx->streams[i];
		p_out_stream = avformat_new_stream(p_ofmt_ctx, p_in_stream->codec->codec);
		if (NULL == p_out_stream)
		{
			printf("Failed allocation output AVStream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		///< 复制AVCodecContext的设置（Copy the setting of AVCodecContext）
		if ((ret = avcodec_copy_context(p_out_stream->codec, p_in_stream->codec)) < 0)
		{
			printf("Failed to copy context from input to output stream docec context\n");
			goto end;
		}
		p_out_stream->codec->codec_tag = 0;
		if (p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			p_out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	///< 输出格式
	av_dump_format(p_ofmt_ctx, 0, p_out_fname, 1);

	///< 打开输出文件（Open output file）
	if (!(p_ofmt->flags & AVFMT_NOFILE))
	{
		if ((ret = avio_open(&p_ofmt_ctx->pb, p_out_fname, AVIO_FLAG_WRITE)) < 0)
		{
			printf("Could not open output file '%s'", p_out_fname);
			goto end;
		}
	}

	///< 写入文件头（Write file header）
	if ((ret = avformat_write_header(p_ofmt_ctx, NULL)) < 0)
	{
		printf("Error occurred when opening output file\n");
		goto end;
	}
	int frame_index = 0;
	while (true)
	{
		AVStream *p_instrm = NULL, *p_outstrm = NULL;

		///< 获取一个AVPacket（Get an AVPacket）
		if ((ret = av_read_frame(p_ifmt_ctx, &pkt)) < 0)
		{
			break;
		}
		p_instrm = p_ifmt_ctx->streams[pkt.stream_index];
		p_outstrm = p_ofmt_ctx->streams[pkt.stream_index];

		///< 转换PTS/DTS（Convert PTS/DTS）
		pkt.pts = av_rescale_q_rnd(pkt.pts, p_instrm->time_base,
			p_outstrm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, p_instrm->time_base,
			p_outstrm->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = static_cast<int>(av_rescale_q(pkt.duration,
			p_instrm->time_base, p_outstrm->time_base));
		pkt.pos = -1;

		///< 写入（Write）
		if ((ret = av_interleaved_write_frame(p_ofmt_ctx, &pkt)) < 0)
		{
			printf("Error muxing packet\n");
			break;
		}
		printf("Write %8d frames to output file \n", frame_index++);
		av_free_packet(&pkt);
	}

	///< 写入文件尾
	av_write_trailer(p_ofmt_ctx);

end:
	avformat_close_input(&p_ifmt_ctx);
	///< 关闭输出
	if (p_ofmt_ctx && !(p_ofmt_ctx->flags & AVFMT_NOFILE))
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
