// save_frame_jpeg.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <math.h>
#include <windows.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <jpeglib.h>
#include <setjmp.h>

#define inline __inline
#define snprintf _snprintf

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096


#pragma comment (lib, "Ws2_32.lib")  
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avdevice.lib")
#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swresample.lib")
#pragma comment (lib, "swscale.lib")
#pragma comment(lib, "jpeg.lib")

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);

static void yuv420p_save(AVFrame *pFrame, int width, int height, int iFrame);

static void bmp_save(AVFrame *pFrame, int width, int height, int iFrame);

static void saveFrame_jpg(uint8_t *pRGBBuffer, int iFrame, int width, int height);

int _tmain(int argc, _TCHAR* argv[])
{
	AVFormatContext *pFormatCtx = NULL;
	char            *fileName = "E:\\aa.rm";
	int             i, videoStream;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame;
	AVFrame         *pFrameRGB;
	int             numBytes;
	uint8_t         *buffer;
	AVPacket        packet;
	int             frameFinished;
	struct SwsContext *img_convert_ctx = NULL;

	av_register_all();

	if (avformat_open_input(&pFormatCtx, fileName, NULL, NULL) != 0)
	{
		printf("can not open");
		return -1;
	}

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("can not find");
		return -1;
	}

	av_dump_format(pFormatCtx, -1, fileName, 0);

	videoStream = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			break;
		}
	}

	if (videoStream == -1)
	{
		printf("not find videoStream");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		return -1;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		return -1;
	}

	pFrame = av_frame_alloc();
	if (pFrame == NULL)
	{
		return -1;
	}

	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
	{
		return -1;
	}

	numBytes = avpicture_get_size(PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);

	buffer = av_malloc(numBytes);

	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);


	img_convert_ctx = sws_getCachedContext(img_convert_ctx, pCodecCtx->width, pCodecCtx->height,
		pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		if (packet.stream_index == videoStream)
		{
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (frameFinished)
			{

				if (!img_convert_ctx)
				{
					fprintf(stderr, "Cannot initialize sws conversion context\n");
					exit(1);
				}

				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameRGB->data, pFrameRGB->linesize);

				if (i++<5)
				{
					//SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
					//yuv420p_save(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
					//bmp_save(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
					saveFrame_jpg(pFrameRGB->data[0], i, pCodecCtx->width, pCodecCtx->height);

				}
			}
		}
		if (i > 5)
		{
			break;
		}
	}
	av_free_packet(&packet);

	av_free(buffer);
	av_free(pFrame);
	av_free(pFrameRGB);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);


	return 0;
}

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
	FILE *pFile;
	char szFilename[32];
	int y;

	sprintf(szFilename, "frame%d.jpg", iFrame);
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
	sprintf(szFilename, "frame%d.jpg", iFrame);
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
	sprintf(szFilename, "frame%d.bmp", iFrame);
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


	sprintf(szFilename, "test%d.jpg", iFrame);//图片名字为视频名+号码
	fp = fopen(szFilename, "wb");

	if (fp == NULL)
		return;

	jpeg_stdio_dest(&cinfo, fp);

	cinfo.image_width = width;    // 为图的宽和高，单位为像素 
	cinfo.image_height = height;
	cinfo.input_components = 3;   // 在此为1,表示灰度图， 如果是彩色位图，则为3 
	cinfo.in_color_space = JCS_RGB; //JCS_GRAYSCALE表示灰度图，JCS_RGB表示彩色图像

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 80, 1);

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
