/**
 * ============================================================================
 * @file	pic_convert.cpp
 *
 * @brief   本文记录的程序将像素格式为YUV420P，分辨率为480x272的视频转换为
 *			像素格式为RGB24，分辨率为1280x720的视频。
 * @details 
 *
 * @version 1.0  
 * @date	2015/09/08 14:40
 *
 * @author  志杰, shizhijie@baofeng.com
 * ============================================================================
 */

#include "stdafx.h"

extern "C"
{
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 3)
	{
		printf("usage: %s in_file out_file\n", argv[0]);
		return -1;
	}

	///< Parameters
	FILE *p_src_file = fopen(argv[1], "rb");
	const int src_w = 480, src_h = 272;
	AVPixelFormat src_pixfmt = AV_PIX_FMT_YUV420P;
	int src_bpp = av_get_bits_per_pixel(av_pix_fmt_desc_get(src_pixfmt));

	FILE *p_dst_file = fopen(argv[2], "wb");
	const int dst_w = 1280, dst_h = 720;
	AVPixelFormat dst_pixfmt = AV_PIX_FMT_RGB24;
	int dst_bpp = av_get_bits_per_pixel(av_pix_fmt_desc_get(dst_pixfmt));

	///< Structures
	uint8_t *src_data[4] = { NULL };
	int src_linesize[4] = { 0 };

	uint8_t *dst_data[4] = { NULL };
	int dst_linesize[4] = { 0 };

	int rescale_method = SWS_BICUBIC;
	struct SwsContext *p_img_convert_ctx = NULL;
	uint8_t *tmp_buf = (uint8_t *)malloc(src_w * src_h * src_bpp / 8);

	int frame_idx = 0, ret = 0;
	if ((ret = av_image_alloc(src_data, src_linesize, src_w, src_h, src_pixfmt, 1)) < 0)
	{
		printf("Could not allocate source image\n");
		return -1;
	}
	if ((ret = av_image_alloc(dst_data, dst_linesize, dst_w, dst_h, dst_pixfmt, 1)) < 0)
	{
		printf("Could not allocate destination image\n");
		return -1;
	}

	///< Init method 1
	p_img_convert_ctx = sws_alloc_context();
	///< Show AVOption
	av_opt_show2(p_img_convert_ctx, stdout, AV_OPT_FLAG_VIDEO_PARAM, 0);
	///< Set value
	av_opt_set_int(p_img_convert_ctx, "sws_flags", SWS_BICUBIC | SWS_PRINT_INFO, 0);
	av_opt_set_int(p_img_convert_ctx, "srcw", src_w, 0);
	av_opt_set_int(p_img_convert_ctx, "srch", src_h, 0);
	av_opt_set_int(p_img_convert_ctx, "src_format", src_pixfmt, 0);
	///< '0' for MPEG (Y:0-235); '1' for JPEG (Y:0-255)
	av_opt_set_int(p_img_convert_ctx, "src_range", 1, 0);
	av_opt_set_int(p_img_convert_ctx, "dstw", dst_w, 0);
	av_opt_set_int(p_img_convert_ctx, "dsth", dst_h, 0);
	av_opt_set_int(p_img_convert_ctx, "dst_format", dst_pixfmt, 0);
	av_opt_set_int(p_img_convert_ctx, "dst_range", 1, 0);
	sws_init_context(p_img_convert_ctx, NULL, NULL);

	/////< Init method 2
	//p_img_convert_ctx = sws_getContext(src_w, src_h, src_pixfmt, dst_w, dst_h,
	//	dst_pixfmt, rescale_method, NULL, NULL, NULL);

	///< Colorspace
	if (-1 == (ret = sws_setColorspaceDetails(p_img_convert_ctx,
		sws_getCoefficients(SWS_CS_ITU601), 0, sws_getCoefficients(SWS_CS_ITU709),
		0, 0, 1 << 16, 1 << 16)))
	{
		printf("Colorspace not support.\n");
		return -1;
	}

	while (true)
	{
		if (fread(tmp_buf, 1, src_w * src_h * src_bpp / 8, p_src_file) !=
			src_w * src_h * src_bpp / 8)
		{
			break;
		}

		switch (src_pixfmt)
		{
		case AV_PIX_FMT_GRAY8:
			memcpy(src_data[0], tmp_buf, src_w * src_h);
			break;
		case AV_PIX_FMT_YUV420P:
			memcpy(src_data[0], tmp_buf, src_w * src_h); ///< Y
			memcpy(src_data[1], tmp_buf + src_w * src_h, src_w * src_h / 4); ///< U
			memcpy(src_data[2], tmp_buf + src_w * src_h * 5 / 4, src_w * src_h / 4); /// V
			break;
		case AV_PIX_FMT_YUV422P:
			memcpy(src_data[0], tmp_buf, src_w * src_h); ///< Y
			memcpy(src_data[1], tmp_buf + src_w * src_h, src_w * src_h / 2); ///< U
			memcpy(src_data[1], tmp_buf + src_w * src_h * 3 / 2, src_w * src_h / 2); ///< V
			break;
		case AV_PIX_FMT_YUV444P:
			memcpy(src_data[0], tmp_buf, src_w * src_h); ///< Y
			memcpy(src_data[1], tmp_buf + src_w * src_h, src_w * src_h); ///< U
			memcpy(src_data[1], tmp_buf + src_w * src_h * 2, src_w * src_h); ///< V
			break;
		case AV_PIX_FMT_YUYV422:
			memcpy(src_data[0], tmp_buf, src_w * src_h * 2); ///< Packet
			break;
		case AV_PIX_FMT_RGB24:
			memcpy(src_data[0], tmp_buf, src_w * src_h * 3); ///< Packet
			break;
		default:
			printf("Not Support Input Pixel Format.\n");
			break;
		}

		sws_scale(p_img_convert_ctx, src_data, src_linesize, 0,
			src_h, dst_data, dst_linesize);
		printf("Finish process frame %5d\n", frame_idx);
		++frame_idx;

		switch (dst_pixfmt)
		{
		case AV_PIX_FMT_GRAY8:
			fwrite(dst_data[0], 1, dst_w * dst_h, p_dst_file);
			break;
		case AV_PIX_FMT_YUV420P:
			fwrite(dst_data[0], 1, dst_w * dst_h, p_dst_file); ///< Y
			fwrite(dst_data[1], 1, dst_w * dst_h / 4, p_dst_file); ///< U
			fwrite(dst_data[2], 1, dst_w * dst_h / 4, p_dst_file); ///< V
			break;
		case AV_PIX_FMT_YUV422P:
			fwrite(dst_data[0], 1, dst_w * dst_h, p_dst_file); ///< Y
			fwrite(dst_data[1], 1, dst_w * dst_h / 2, p_dst_file); ///< U
			fwrite(dst_data[2], 1, dst_w * dst_h / 2, p_dst_file); ///< V
			break;
		case AV_PIX_FMT_YUV444P:
			fwrite(dst_data[0], 1, dst_w * dst_h, p_dst_file); ///< Y
			fwrite(dst_data[1], 1, dst_w * dst_h, p_dst_file); ///< U
			fwrite(dst_data[2], 1, dst_w * dst_h, p_dst_file); ///< V
			break;
		case AV_PIX_FMT_YUYV422:
			fwrite(dst_data[0], 1, dst_w * dst_h * 2, p_dst_file); ///< Packed
			break;
		case AV_PIX_FMT_RGB24:
			fwrite(dst_data[0], 1, dst_w * dst_h * 3, p_dst_file); ///< Packed
			break;
		default:
			printf("Not Support Output Pixel Format.\n");
			break;
		}
	}

	sws_freeContext(p_img_convert_ctx);
	free(tmp_buf); tmp_buf = NULL;
	fclose(p_src_file); p_src_file = NULL;
	fclose(p_dst_file); p_dst_file = NULL;
	av_freep(&src_data[0]);
	av_freep(&dst_data[0]);

	return 0;
}
