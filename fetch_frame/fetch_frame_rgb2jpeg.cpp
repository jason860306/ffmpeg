// fetch_frame_rgb2jpeg.cpp : 定义控制台应用程序的入口点。
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

#include "jpeglib.h"

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);
static void yuv420p_save(AVFrame *pFrame, int width, int height, int iFrame);
static void bmp_save(AVFrame *pFrame, int width, int height, int iFrame);
static void saveFrame_jpg(uint8_t *pRGBBuffer, int iFrame, int width, int height);

namespace myffmpeg
{
	/* "user interface" functions */
	static void dump_stream_format(AVFormatContext *ic, int i,
		int index, int is_output);
	void av_dump_format(AVFormatContext *ic, int index,
		const char *url, int is_output);
	static void print_fps(double d, const char *postfix);
	static void dump_metadata(void *ctx, AVDictionary *m, const char *indent);
	/* param change side data*/
	static void dump_paramchange(void *ctx, AVPacketSideData *sd);
	static void dump_sidedata(void *ctx, AVStream *st, const char *indent);
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2)
	{
		printf("usage: %s vide_infile\n", argv[0]);
		return -1;
	}
	SwsContext *pSWSCtx;
	AVFormatContext *pFormatCtx;
	const char *vide_outfile = argv[1], *img_outfile = argv[1];
	char szImgFName[128] = { 0 };
	int i, videoStream;
	AVCodecContext *pCodecCtx;
	AVFrame *pFrame;
	AVFrame *pFrameRGB;
	int     numBytes, frameFinished;
	uint8_t *buffer;
	static AVPacket packet;

	av_register_all();

	if (NULL == (pFormatCtx = avformat_alloc_context()))
	{
		printf("error!\n");
		return -1;
	}
#if 0
	- if ((ret = av_open_input_file(&movie->format_ctx, movie->file_name, iformat, 0, NULL)) < 0) {
	+ if ((ret = avformat_open_input(&movie->format_ctx, movie->file_name, iformat, NULL)) < 0) {
#endif
	if (avformat_open_input(&pFormatCtx, vide_outfile, NULL, NULL) != 0)
	{
		printf("error!\n");
		return -1;
	}

	av_log(NULL, AV_LOG_INFO, "format: %s (%s)\n", pFormatCtx->iformat->name,
		pFormatCtx->iformat->long_name);

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("error!\n");
		return -1;
	}

#if 0
	av_log(NULL, AV_LOG_INFO, "  Duration: ");
	if (ic->duration != AV_NOPTS_VALUE) {
		int hours, mins, secs, us;
		int64_t duration = ic->duration + 5000;
		secs = duration / AV_TIME_BASE;
		us = duration % AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;
		av_log(NULL, AV_LOG_INFO, "%02d:%02d:%02d.%02d", hours, mins, secs,
			(100 * us) / AV_TIME_BASE);
	}
	else {
		av_log(NULL, AV_LOG_INFO, "N/A");
	}
#endif
	int64_t duration = 0;
	if (pFormatCtx->duration != AV_NOPTS_VALUE)
	{
		duration = pFormatCtx->duration + 5000;
		duration /= AV_TIME_BASE; // sec
	}
	av_log(NULL, AV_LOG_INFO, "duration: %lld, ", duration);

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

		const char *codec_type = av_get_media_type_string(pCodecCtx->codec_type);
		const char *codec_name = avcodec_get_name(pCodecCtx->codec_id);
		av_log(NULL, AV_LOG_INFO, "%s: %s, ", codec_type, codec_name);
		if (pCodecCtx->codec && strcmp(pCodecCtx->codec->name, codec_name))
		{
			av_log(NULL, AV_LOG_INFO, " (%s)", pCodecCtx->codec->name);
		}
#if 0
		codec_type = av_get_media_type_string(enc->codec_type);
		codec_name = avcodec_get_name(enc->codec_id);

		snprintf(buf, buf_size, "%s: %s", codec_type ? codec_type : "unknown",
			codec_name);
		buf[0] ^= 'a' ^ 'A'; /* first letter in uppercase */

		if (enc->codec && strcmp(enc->codec->name, codec_name))
			snprintf(buf + strlen(buf), buf_size - strlen(buf), " (%s)", enc->codec->name);
#endif
	}
	if (videoStream == -1)
	{
		printf("error! Didn't find a video stream\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	int width = pCodecCtx->width, height = pCodecCtx->height;
	av_log(NULL, AV_LOG_INFO, "video size: %dx%d", width, height);
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
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

	//int dst_width = 1920, dst_height = 1080;
	int dst_width = 320, dst_height = 240;
	//int dst_width = 1024, dst_height = 768;
	numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, dst_width, dst_height, 1);
	buffer = new uint8_t[numBytes];
#if 0
	int avpicture_fill(AVPicture *picture, const uint8_t *ptr,  
		enum AVPixelFormat pix_fmt, int width, int height)
	{
		return av_image_fill_arrays(picture->data, picture->linesize,
			ptr, pix_fmt, width, height, 1);
	}
#endif
	
		av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer,
			AV_PIX_FMT_RGB24, dst_width, dst_height, 1);
		pSWSCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
			pCodecCtx->pix_fmt, dst_width, dst_height, AV_PIX_FMT_RGB24,
			SWS_BICUBIC, NULL, NULL, NULL);

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
					sws_scale(pSWSCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
						pFrameRGB->data, pFrameRGB->linesize);

					//SaveFrame(pFrameRGB, dst_width, dst_height, i);
					//yuv420p_save(pFrameRGB, dst_width, dst_height, i);  
					//bmp_save(pFrameRGB, dst_width, dst_height, i);  
					saveFrame_jpg(pFrameRGB->data[0], i, dst_width, dst_height);
#if 0
					memset(szImgFName, 0, sizeof(szImgFName));
					sprintf_s(szImgFName, sizeof(szImgFName), "%s_%03d.jpg", img_outfile, i);
					FILE *pImgFile = NULL;
					if (0 != fopen_s(&pImgFile, szImgFName, "wb"))
					{
						printf("error!\n");
						return -1;
					}
					for (int y = 0; y < pCodecCtx->height; ++y)
					{
						fwrite(pFrameRGB->data[0] + y * pFrameRGB->linesize[0], 1, pCodecCtx->width * 3, pImgFile);
					}
					// fwrite(pFrameRGB->data[0], 1, pCodecCtx->width * pCodecCtx->height * 3, pImgFile);
					fclose(pImgFile); pImgFile = NULL;
#endif
					i++;
				}
			}
		}
		av_packet_unref(&packet);
	}
	printf("\nkey-frame numbers: %ld", i);

	av_free(pFrameRGB);
	av_free(pFrame);
	sws_freeContext(pSWSCtx);
	avcodec_close(pCodecCtx);
#if 0
	- av_close_input_file(ic);
	+ avformat_close_input(&ic);
#endif
	avformat_close_input(&pFormatCtx);

	system("pause");

	return 0;
}

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
	FILE *pFile;
	char szFilename[32];
	int y;

	sprintf(szFilename, "frame%06d.jpg", iFrame);
	pFile = fopen(szFilename, "wb");
	if (!pFile)
	{
		return;
	}

	fprintf(pFile, "P6\n%d %d\n255\n", width, height);
	for (y = 0; y<height; y++)
	{
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);
	}

	fclose(pFile);
}

static void yuv420p_save(AVFrame *pFrame, int width, int height, int iFrame)
{
	int i = 0;
	FILE *pFile;
	char szFilename[32];

	int height_half = height / 2, width_half = width / 2;
	int y_wrap = pFrame->linesize[0];
	int u_wrap = pFrame->linesize[1];
	int v_wrap = pFrame->linesize[2];

	unsigned char *y_buf = pFrame->data[0];
	unsigned char *u_buf = pFrame->data[1];
	unsigned char *v_buf = pFrame->data[2];
	if (NULL == y_buf || NULL == u_buf || NULL == v_buf)
	{
		return;
	}
	sprintf(szFilename, "frame%06d.jpg", iFrame);
	pFile = fopen(szFilename, "wb");

	//save y  
	for (i = 0; i < height; i++)
		fwrite(y_buf + i * y_wrap, 1, width, pFile);
	fprintf(stderr, "===>save Y success\n");

	//save u  
	for (i = 0; i < height_half; i++)
		fwrite(u_buf + i * u_wrap, 1, width_half, pFile);
	fprintf(stderr, "===>save U success\n");

	//save v  
	for (i = 0; i < height_half; i++)
		fwrite(v_buf + i * v_wrap, 1, width_half, pFile);
	fprintf(stderr, "===>save V success\n");

	fflush(pFile);
	fclose(pFile);
}

static void bmp_save(AVFrame *pFrame, int width, int height, int iFrame)
{
	BITMAPFILEHEADER bmpheader;
	BITMAPINFO bmpinfo;
	int y = 0;
	FILE *pFile;
	char szFilename[32];

	unsigned char *y_buf = pFrame->data[0];
	sprintf(szFilename, "frame%06d.bmp", iFrame);
	pFile = fopen(szFilename, "wb");

	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width*height * 24 / 8;

	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.bmiHeader.biWidth = width;
	bmpinfo.bmiHeader.biHeight = -height;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = 24;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biXPelsPerMeter = 100;
	bmpinfo.bmiHeader.biYPelsPerMeter = 100;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(BITMAPFILEHEADER), 1, pFile);
	fwrite(&bmpinfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, pFile);

	fwrite(pFrame->data[0], width*height * 24 / 8, 1, pFile);

	/*for(y=0; y<height; y++)
	{
	fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width*3, pFile);
	}*/

	//fflush(pFile);  
	fclose(pFile);
}

static void saveFrame_jpg(uint8_t *pRGBBuffer, int iFrame, int width, int height)
{

	struct jpeg_compress_struct cinfo;

	struct jpeg_error_mgr jerr;

	char szFilename[1024];
	int row_stride;

	FILE *fp;

	JSAMPROW row_pointer[1];   // 一行位图  

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);


	sprintf(szFilename, "test%06d.jpg", iFrame);//图片名字为视频名+号码  
	fp = fopen(szFilename, "wb");

	if (fp == NULL)
		return;

	jpeg_stdio_dest(&cinfo, fp);

	cinfo.image_width = width;    // 为图的宽和高，单位为像素   
	cinfo.image_height = height;
	cinfo.input_components = 3;   // 在此为1,表示灰度图， 如果是彩色位图，则为3   
	cinfo.in_color_space = JCS_YCbCr; //JCS_GRAYSCALE表示灰度图，JCS_RGB表示彩色图像  

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 80, 1);

	jpeg_calc_jpeg_dimensions(&cinfo);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = cinfo.image_width * 3;//每一行的字节数,如果不是索引图,此处需要乘以3  

	// 对每一行进行压缩  
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &(pRGBBuffer[cinfo.next_scanline * row_stride]);
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(fp);
}

#if 0
static void dump_sidedata(void *ctx, AVStream *st, const char *indent)
{
	int i;

	if (st->nb_side_data)
		av_log(ctx, AV_LOG_INFO, "%sSide data:\n", indent);

	for (i = 0; i < st->nb_side_data; i++) {
		AVPacketSideData sd = st->side_data[i];
		av_log(ctx, AV_LOG_INFO, "%s  ", indent);

		switch (sd.type) {
		case AV_PKT_DATA_PALETTE:
			av_log(ctx, AV_LOG_INFO, "palette");
			break;
		case AV_PKT_DATA_NEW_EXTRADATA:
			av_log(ctx, AV_LOG_INFO, "new extradata");
			break;
		case AV_PKT_DATA_PARAM_CHANGE:
			av_log(ctx, AV_LOG_INFO, "paramchange: ");
			dump_paramchange(ctx, &sd);
			break;
		case AV_PKT_DATA_H263_MB_INFO:
			av_log(ctx, AV_LOG_INFO, "h263 macroblock info");
			break;
		case AV_PKT_DATA_REPLAYGAIN:
			av_log(ctx, AV_LOG_INFO, "replaygain: ");
			dump_replaygain(ctx, &sd);
			break;
		case AV_PKT_DATA_DISPLAYMATRIX:
			av_log(ctx, AV_LOG_INFO, "displaymatrix: rotation of %.2f degrees",
				av_display_rotation_get((int32_t *)sd.data));
			break;
		case AV_PKT_DATA_STEREO3D:
			av_log(ctx, AV_LOG_INFO, "stereo3d: ");
			dump_stereo3d(ctx, &sd);
			break;
		case AV_PKT_DATA_AUDIO_SERVICE_TYPE:
			av_log(ctx, AV_LOG_INFO, "audio service type: ");
			dump_audioservicetype(ctx, &sd);
			break;
		default:
			av_log(ctx, AV_LOG_WARNING,
				"unknown side data type %d (%d bytes)", sd.type, sd.size);
			break;
		}

		av_log(ctx, AV_LOG_INFO, "\n");
	}
}

/* param change side data*/
static void dump_paramchange(void *ctx, AVPacketSideData *sd)
{
	int size = sd->size;
	const uint8_t *data = sd->data;
	uint32_t flags, channels, sample_rate, width, height;
	uint64_t layout;

	if (!data || sd->size < 4)
		goto fail;

	flags = AV_RL32(data);
	data += 4;
	size -= 4;

	if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT) {
		if (size < 4)
			goto fail;
		channels = AV_RL32(data);
		data += 4;
		size -= 4;
		av_log(ctx, AV_LOG_INFO, "channel count %"PRIu32", ", channels);
	}
	if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT) {
		if (size < 8)
			goto fail;
		layout = AV_RL64(data);
		data += 8;
		size -= 8;
		av_log(ctx, AV_LOG_INFO,
			"channel layout: %s, ", av_get_channel_name(layout));
	}
	if (flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE) {
		if (size < 4)
			goto fail;
		sample_rate = AV_RL32(data);
		data += 4;
		size -= 4;
		av_log(ctx, AV_LOG_INFO, "sample_rate %"PRIu32", ", sample_rate);
	}
	if (flags & AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS) {
		if (size < 8)
			goto fail;
		width = AV_RL32(data);
		data += 4;
		size -= 4;
		height = AV_RL32(data);
		data += 4;
		size -= 4;
		av_log(ctx, AV_LOG_INFO, "width %"PRIu32" height %"PRIu32, width, height);
	}

	return;
fail:
	av_log(ctx, AV_LOG_INFO, "unknown param");
}
static void print_fps(double d, const char *postfix)
{
	uint64_t v = lrintf(d * 100);
	if (!v)
		av_log(NULL, AV_LOG_INFO, "%1.4f %s", d, postfix);
	else if (v % 100)
		av_log(NULL, AV_LOG_INFO, "%3.2f %s", d, postfix);
	else if (v % (100 * 1000))
		av_log(NULL, AV_LOG_INFO, "%1.0f %s", d, postfix);
	else
		av_log(NULL, AV_LOG_INFO, "%1.0fk %s", d / 1000, postfix);
}

static void dump_metadata(void *ctx, AVDictionary *m, const char *indent)
{
	if (m && !(av_dict_count(m) == 1 && av_dict_get(m, "language", NULL, 0))) {
		AVDictionaryEntry *tag = NULL;

		av_log(ctx, AV_LOG_INFO, "%sMetadata:\n", indent);
		while ((tag = av_dict_get(m, "", tag, AV_DICT_IGNORE_SUFFIX)))
		if (strcmp("language", tag->key)) {
			const char *p = tag->value;
			av_log(ctx, AV_LOG_INFO,
				"%s  %-16s: ", indent, tag->key);
			while (*p) {
				char tmp[256];
				size_t len = strcspn(p, "\x8\xa\xb\xc\xd");
				av_strlcpy(tmp, p, FFMIN(sizeof(tmp), len+1));
				av_log(ctx, AV_LOG_INFO, "%s", tmp);
				p += len;
				if (*p == 0xd) av_log(ctx, AV_LOG_INFO, " ");
				if (*p == 0xa) av_log(ctx, AV_LOG_INFO, "\n%s  %-16s: ", indent, "");
				if (*p) p++;
			}
			av_log(ctx, AV_LOG_INFO, "\n");
		}
	}
}

/* "user interface" functions */
static void dump_stream_format(AVFormatContext *ic, int i,
	int index, int is_output)
{
	char buf[256];
	int flags = (is_output ? ic->oformat->flags : ic->iformat->flags);
	AVStream *st = ic->streams[i];
	AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
	char *separator = ic->dump_separator;
	char **codec_separator = av_opt_ptr(st->codec->av_class, st->codec, "dump_separator");
	int use_format_separator = !*codec_separator;

	if (use_format_separator)
		*codec_separator = av_strdup(separator);
	avcodec_string(buf, sizeof(buf), st->codec, is_output);
	if (use_format_separator)
		av_freep(codec_separator);
	av_log(NULL, AV_LOG_INFO, "    Stream #%d:%d", index, i);

	/* the pid is an important information, so we display it */
	/* XXX: add a generic system */
	if (flags & AVFMT_SHOW_IDS)
		av_log(NULL, AV_LOG_INFO, "[0x%x]", st->id);
	if (lang)
		av_log(NULL, AV_LOG_INFO, "(%s)", lang->value);
	av_log(NULL, AV_LOG_DEBUG, ", %d, %d/%d", st->codec_info_nb_frames,
		st->time_base.num, st->time_base.den);
	av_log(NULL, AV_LOG_INFO, ": %s", buf);

	if (st->sample_aspect_ratio.num && // default
		av_cmp_q(st->sample_aspect_ratio, st->codec->sample_aspect_ratio)) {
		AVRational display_aspect_ratio;
		av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
			st->codec->width  * st->sample_aspect_ratio.num,
			st->codec->height * st->sample_aspect_ratio.den,
			1024 * 1024);
		av_log(NULL, AV_LOG_INFO, ", SAR %d:%d DAR %d:%d",
			st->sample_aspect_ratio.num, st->sample_aspect_ratio.den,
			display_aspect_ratio.num, display_aspect_ratio.den);
	}

	if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
		int fps = st->avg_frame_rate.den && st->avg_frame_rate.num;
		int tbr = st->r_frame_rate.den && st->r_frame_rate.num;
		int tbn = st->time_base.den && st->time_base.num;
		int tbc = st->codec->time_base.den && st->codec->time_base.num;

		if (fps || tbr || tbn || tbc)
			av_log(NULL, AV_LOG_INFO, "%s", separator);

		if (fps)
			print_fps(av_q2d(st->avg_frame_rate), tbr || tbn || tbc ? "fps, " : "fps");
		if (tbr)
			print_fps(av_q2d(st->r_frame_rate), tbn || tbc ? "tbr, " : "tbr");
		if (tbn)
			print_fps(1 / av_q2d(st->time_base), tbc ? "tbn, " : "tbn");
		if (tbc)
			print_fps(1 / av_q2d(st->codec->time_base), "tbc");
	}

	if (st->disposition & AV_DISPOSITION_DEFAULT)
		av_log(NULL, AV_LOG_INFO, " (default)");
	if (st->disposition & AV_DISPOSITION_DUB)
		av_log(NULL, AV_LOG_INFO, " (dub)");
	if (st->disposition & AV_DISPOSITION_ORIGINAL)
		av_log(NULL, AV_LOG_INFO, " (original)");
	if (st->disposition & AV_DISPOSITION_COMMENT)
		av_log(NULL, AV_LOG_INFO, " (comment)");
	if (st->disposition & AV_DISPOSITION_LYRICS)
		av_log(NULL, AV_LOG_INFO, " (lyrics)");
	if (st->disposition & AV_DISPOSITION_KARAOKE)
		av_log(NULL, AV_LOG_INFO, " (karaoke)");
	if (st->disposition & AV_DISPOSITION_FORCED)
		av_log(NULL, AV_LOG_INFO, " (forced)");
	if (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
		av_log(NULL, AV_LOG_INFO, " (hearing impaired)");
	if (st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
		av_log(NULL, AV_LOG_INFO, " (visual impaired)");
	if (st->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
		av_log(NULL, AV_LOG_INFO, " (clean effects)");
	av_log(NULL, AV_LOG_INFO, "\n");

	dump_metadata(NULL, st->metadata, "    ");

	dump_sidedata(NULL, st, "    ");
}
void av_dump_format(AVFormatContext *ic, int index,
	const char *url, int is_output)
{
	int i;
	uint8_t *printed = ic->nb_streams ? av_mallocz(ic->nb_streams) : NULL;
	if (ic->nb_streams && !printed)
		return;

	av_log(NULL, AV_LOG_INFO, "%s #%d, %s, %s '%s':\n",
		is_output ? "Output" : "Input",
		index,
		is_output ? ic->oformat->name : ic->iformat->name,
		is_output ? "to" : "from", url);
	dump_metadata(NULL, ic->metadata, "  ");

	if (!is_output) {
		av_log(NULL, AV_LOG_INFO, "  Duration: ");
		if (ic->duration != AV_NOPTS_VALUE) {
			int hours, mins, secs, us;
			int64_t duration = ic->duration + 5000;
			secs = duration / AV_TIME_BASE;
			us = duration % AV_TIME_BASE;
			mins = secs / 60;
			secs %= 60;
			hours = mins / 60;
			mins %= 60;
			av_log(NULL, AV_LOG_INFO, "%02d:%02d:%02d.%02d", hours, mins, secs,
				(100 * us) / AV_TIME_BASE);
		}
		else {
			av_log(NULL, AV_LOG_INFO, "N/A");
		}
		if (ic->start_time != AV_NOPTS_VALUE) {
			int secs, us;
			av_log(NULL, AV_LOG_INFO, ", start: ");
			secs = ic->start_time / AV_TIME_BASE;
			us = abs(ic->start_time % AV_TIME_BASE);
			av_log(NULL, AV_LOG_INFO, "%d.%06d",
				secs, (int)av_rescale(us, 1000000, AV_TIME_BASE));
		}
		av_log(NULL, AV_LOG_INFO, ", bitrate: ");
		if (ic->bit_rate)
			av_log(NULL, AV_LOG_INFO, "%d kb/s", ic->bit_rate / 1000);
		else
			av_log(NULL, AV_LOG_INFO, "N/A");
		av_log(NULL, AV_LOG_INFO, "\n");
	}

	for (i = 0; i < ic->nb_chapters; i++) {
		AVChapter *ch = ic->chapters[i];
		av_log(NULL, AV_LOG_INFO, "    Chapter #%d:%d: ", index, i);
		av_log(NULL, AV_LOG_INFO,
			"start %f, ", ch->start * av_q2d(ch->time_base));
		av_log(NULL, AV_LOG_INFO,
			"end %f\n", ch->end * av_q2d(ch->time_base));

		dump_metadata(NULL, ch->metadata, "    ");
	}

	if (ic->nb_programs) {
		int j, k, total = 0;
		for (j = 0; j < ic->nb_programs; j++) {
			AVDictionaryEntry *name = av_dict_get(ic->programs[j]->metadata,
				"name", NULL, 0);
			av_log(NULL, AV_LOG_INFO, "  Program %d %s\n", ic->programs[j]->id,
				name ? name->value : "");
			dump_metadata(NULL, ic->programs[j]->metadata, "    ");
			for (k = 0; k < ic->programs[j]->nb_stream_indexes; k++) {
				dump_stream_format(ic, ic->programs[j]->stream_index[k],
					index, is_output);
				printed[ic->programs[j]->stream_index[k]] = 1;
			}
			total += ic->programs[j]->nb_stream_indexes;
		}
		if (total < ic->nb_streams)
			av_log(NULL, AV_LOG_INFO, "  No Program\n");
	}

	for (i = 0; i < ic->nb_streams; i++)
	if (!printed[i])
		dump_stream_format(ic, i, index, is_output);

	av_free(printed);
}
#endif
