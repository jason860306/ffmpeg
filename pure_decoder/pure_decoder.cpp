/**
* ============================================================================
* @file	pure_decoder.h
*
* @brief	一个“纯净”的解码器
* @details	它仅仅通过调用libavcodec将H.264/HEVC等格式的压缩视频码流解码成为YUV数据。
*			此解码器的输入必须是只包含视频编码数据“裸流”（例如H.264、HEVC码流文件）
*
* @version 1.0
* @date	2014-12-20 22:37:14
*
* @author  shizhijie, shizhijie@baofeng.com
* ============================================================================
*/

#include "stdafx.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#ifdef __cplusplus
}
#endif // __cplusplus

#define USE_SWSCALE 1

///< test different codec
#define TEST_H264 0
#define TEST_HEVC 1

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 3)
	{
		printf("usage: %s in_file out_file\n", argv[0]);
		return -1;
	}

	AVCodec *p_codec = NULL;
	AVCodecContext *p_codec_ctx = NULL;
	AVCodecParserContext *p_codec_parser_ctx = NULL;

	FILE *p_fin = NULL, *p_fout = NULL;
	AVFrame *p_frame = NULL;

	const int in_buf_size = 4096;
	uint8_t in_buf[in_buf_size + FF_INPUT_BUFFER_PADDING_SIZE] = { 0 };
	uint8_t *p_cur = NULL;
	int cur_size = 0;
	AVPacket packet;
	int ret = 0, got_picture = 0, y_size = 0;

	const char *p_in_fpath = argv[1];
	enum AVCodecID codec_id = AV_CODEC_ID_NONE;
#if TEST_HEVC
	codec_id = AV_CODEC_ID_HEVC;
#elif TEST_H264
	codec_id = AV_CODEC_ID_H264;
#else
	codec_id = AV_CODEC_ID_MPEG2VIDEO;
#endif

	const char *p_out_fpath = argv[2];
	int first_time = 1;

#if USE_SWSCALE
	struct SwsContext *p_img_convert_ctx = NULL;
	AVFrame *p_frame_yuv = NULL;
	uint8_t *p_out_buf = NULL;
#endif
	av_log_set_level(AV_LOG_DEBUG);

	avcodec_register_all();

	if (NULL == (p_codec = avcodec_find_decoder(codec_id)))
	{
		printf("Codec(: %d) not found\n", static_cast<int>(codec_id));
		return -1;
	}

	if (NULL == (p_codec_ctx = avcodec_alloc_context3(p_codec)))
	{
		printf("Could not allocate video codec context\n");
		return -1;
	}

	if (NULL == (p_codec_parser_ctx = av_parser_init(codec_id)))
	{
		printf("Could not allocate video parser context\n");
		return -1;
	}

	if (p_codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		/**
		 * @brief we do not send complete frames
		 */
		p_codec_ctx->flags |= CODEC_FLAG_TRUNCATED;
	}

	if (avcodec_open2(p_codec_ctx, p_codec, NULL) < 0)
	{
		printf("Could not open codec\n");
		return -1;
	}

	/**
	 * @brief Input file
	 */
	if (NULL == (p_fin = fopen(p_in_fpath, "rb")))
	{
		printf("Could not open input stream\n");
		return -1;
	}

	/**
	 * @brief Output file
	 */
	if (NULL == (p_fout = fopen(p_out_fpath, "wb")))
	{
		printf("Could not open output YUV file\n");
		return -1;
	}

	p_frame = av_frame_alloc();
	av_init_packet(&packet);

	while (true)
	{
		if (0 == (cur_size = fread(in_buf, 1, in_buf_size, p_fin)))
		{
			break;
		}
		p_cur = in_buf;

		while (cur_size > 0)
		{
			int len = av_parser_parse2(p_codec_parser_ctx, p_codec_ctx,
				&packet.data, &packet.size, p_cur, cur_size, AV_NOPTS_VALUE,
				AV_NOPTS_VALUE, AV_NOPTS_VALUE);

			p_cur += len;
			cur_size -= len;

			if (0 == packet.size)
			{
				continue;
			}

			/**
			 * @brief Some info from AVCodecParserContext
			 */
			printf("[Packet]Size: %6d\t", packet.size);
			switch (p_codec_parser_ctx->pict_type)
			{
			case AV_PICTURE_TYPE_I:
				printf("Type: I\t");
				break;
			case AV_PICTURE_TYPE_P:
				printf("Type: P\t");
				break;
			case AV_PICTURE_TYPE_B:
				printf("Type: B\t");
				break;
			default:
				printf("Type: Other\t");
				break;
			}
			printf("Number: %4d\n", p_codec_parser_ctx->output_picture_number);

			if ((ret = avcodec_decode_video2(p_codec_ctx, p_frame,
				&got_picture, &packet) < 0))
			{
				printf("Decode Error.\n");
				return ret;
			}

			if (got_picture)
			{
#if USE_SWSCALE
				if (first_time)
				{
					printf("\nCodec Full Name: %s\n", p_codec_ctx->codec->long_name);
					printf("width: %d\nheight: %d\n\n", p_codec_ctx->width,
						p_codec_ctx->height);
					/**
					 * @brief SwsContext
					 */
					p_img_convert_ctx = sws_getContext(p_codec_ctx->width,
						p_codec_ctx->height, p_codec_ctx->pix_fmt,
						p_codec_ctx->width, p_codec_ctx->height,
						PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

					p_frame_yuv = av_frame_alloc();

					p_out_buf = (uint8_t *)av_malloc(avpicture_get_size(
						PIX_FMT_YUV420P, p_codec_ctx->width, p_codec_ctx->height));
					avpicture_fill((AVPicture *)p_frame_yuv, p_out_buf,
						PIX_FMT_YUV420P, p_codec_ctx->width, p_codec_ctx->height);

					y_size = p_codec_ctx->width * p_codec_ctx->height;

					first_time = 0;
				}
				sws_scale(p_img_convert_ctx, (const uint8_t * const *)p_frame->data,
					p_frame->linesize, 0, p_codec_ctx->height, p_frame_yuv->data,
					p_frame_yuv->linesize);

				fwrite(p_frame_yuv->data[0], 1, y_size, p_fout); ///< Y
				fwrite(p_frame_yuv->data[1], 1, y_size / 4, p_fout); ///< U
				fwrite(p_frame_yuv->data[2], 1, y_size / 4, p_fout);	///< V
#else
				int i = 0;
				uint8_t *tmp_ptr = NULL;
				tmp_ptr = p_frame->data[0];
				for (i = 0; i < p_frame->height, ++i)
				{
					fwrite(tmp_ptr, 1, p_frame->width, p_fout); ///< Y
					tmp_ptr += p_frame->linesize[0];
				}
				tmp_ptr = p_frame->data[1];
				for (i = 0; i < p_frame->height / 2; ++i)
				{
					fwrite(tmp_ptr, 1, p_frame->width / 2, p_fout); ///< U
					tmp_ptr += p_frame->linesize[1];
				}
				tmp_ptr += p_frame->data[2];
				for (i = 0; i < p_frame->height / 2; ++i)
				{
					fwrite(tmp_ptr, 1, p_frame->width / 2, p_fout); ///< V
					tmp_ptr += p_frame->linesize[2];
				}
#endif
				printf("Flush Decoder: Succeed to decode 1 frame!\n");
			}
		}
	}

	/**
	 * @brief Flush Decoder
	 */
	while (true)
	{
		if ((ret = avcodec_decode_video2(p_codec_ctx, p_frame,
			&got_picture, &packet)) < 0)
		{
			printf("Decode Error.\n");
			return ret;
		}
		if (!got_picture)
		{
			break;
		}
		if (got_picture)
		{
#if USE_SWSCALE
			sws_scale(p_img_convert_ctx, (const uint8_t * const *)p_frame->data,
				p_frame->linesize, 0, p_codec_ctx->height, p_frame_yuv->data,
				p_frame_yuv->linesize);

			fwrite(p_frame_yuv->data[0], 1, y_size, p_fout); ///< Y
			fwrite(p_frame_yuv->data[1], 1, y_size / 4, p_fout); ///< U
			fwrite(p_frame_yuv->data[2], 1, y_size / 4, p_fout); ///< V
#else
			int i = 0;
			uint8_t *tmp_ptr = NULL;
			tmp_ptr = p_frame->data[0];
			for (i = 0; i < p_frame->height; ++i)
			{
				fwrite(tmp_ptr, 1, p_frame->width, p_fout); ///< Y
				tmp_ptr += p_frame->linesize[0];
			}
			tmp_ptr = p_frame->data[1];
			for (i = 0; i < p_frame->height / 2; ++i)
			{
				fwrite(tmp_ptr, 1, p_frame->width / 2, p_fout);
				tmp_ptr += p_frame->linesize[1]; ///< U
			}
			tmp_ptr = p_frame->data[2];
			for (i = 0; i < p_frame->height / 2; ++i)
			{
				fwrite(tmp_ptr, 1, p_frame->width / 2, p_fout);
				tmp_ptr += p_frame->linesize[2]; ///< V
			}
#endif
			printf("Flush Decoder: Succeed to decode 1 frame!\n");
		}
	}

	packet.data = NULL;
	packet.size = 0;

	fclose(p_fin); p_fin = NULL;
	fclose(p_fout); p_fout = NULL;

#if USE_SWSCALE
	sws_freeContext(p_img_convert_ctx);
	av_frame_free(&p_frame_yuv);
#endif

	av_parser_close(p_codec_parser_ctx);

	av_frame_free(&p_frame);
	avcodec_close(p_codec_ctx);
	av_free(p_codec_ctx);

	return 0;
}
