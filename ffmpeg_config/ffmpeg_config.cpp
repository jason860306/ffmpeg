// flvparser.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
};

struct URLProtocol;

/**
* @brief Protocol Support Information
*/
const char *url_protocol_info()
{
	const int info_size = 40960;
	static char info[info_size] = { 0 };

	av_register_all();

	struct URLProtocol *url_proto = NULL;

	/**
	* @brief Input
	*/
	struct URLProtocol **tmp_url_proto = &url_proto;
	do
	{
		const char *proto_name = avio_enum_protocols((void **)tmp_url_proto, 0);
		if (NULL != proto_name)
		{
			sprintf_s(info, info_size, "%s[In ][%10s]\n", info,
				proto_name);
		}
	} while (NULL != *tmp_url_proto);
	url_proto = NULL;

	/**
	* @brief Output
	*/
	do
	{
		const char *proto_name = avio_enum_protocols((void **)tmp_url_proto, 1);
		if (NULL != proto_name)
		{
			sprintf_s(info, info_size, "%s[Out][%10s]\n", info,
				proto_name);
		}
	} while (NULL != *tmp_url_proto);
	url_proto = NULL;

	return info;
}

/**
* @brief AVFormat Support Information
*/
const char *avformat_info()
{
	const int info_size = 40960;
	static char info[info_size] = { 0 };

	av_register_all();

	AVInputFormat *tmp_if = av_iformat_next(NULL);
	AVOutputFormat *tmp_of = av_oformat_next(NULL);

	/**
	* @brief Input
	*/
	while (NULL != tmp_if)
	{
		sprintf_s(info, info_size, "%s[In ] %10s\n", info,
			tmp_if->name);
		tmp_if = tmp_if->next;
	}

	/**
	* @brief Output
	*/
	while (NULL != tmp_of)
	{
		sprintf_s(info, info_size, "%s[Out] %10s\n", info,
			tmp_of->name);
		tmp_of = tmp_of->next;
	}

	return info;
}

/**
* @brief AVCodec Support Information
*/
const char *avcodec_info()
{
	const int info_size = 40960;
	static char info[info_size] = { 0 };

	av_register_all();

	AVCodec *tmp_codec = av_codec_next(NULL);

	while (NULL != tmp_codec)
	{
		if (NULL != tmp_codec->decode)
		{
			sprintf_s(info, info_size, "%s[Dec]", info);
		}
		else
		{
			sprintf_s(info, info_size, "%s[Enc]", info);
		}
		switch (tmp_codec->type)
		{
		case AVMEDIA_TYPE_VIDEO:
			sprintf_s(info, info_size, "%s[Video]", info);
			break;
		case AVMEDIA_TYPE_AUDIO:
			sprintf_s(info, info_size, "%s[Audio]", info);
			break;
		default:
			sprintf_s(info, info_size, "%s[Other]", info);
			break;
		}
		sprintf_s(info, info_size, "%s %10s\n", info,
			tmp_codec->name);
		tmp_codec = tmp_codec->next;
	}

	return info;
}

/**
* @brief AVFilter Support Information
*/
const char *avfilter_info()
{
	const int info_size = 40960;
	static char info[info_size] = { 0 };

	av_register_all();

	AVFilter *tmp_filter = (AVFilter *)avfilter_next(NULL);
	while (NULL != tmp_filter)
	{
		sprintf_s(info, info_size, "%s[%10s]\n", info,
			tmp_filter->name);
		tmp_filter = tmp_filter->next;
	}
	return info;
}

/**
* @brief Configuration Information
*/
const char *config_info()
{
	const int info_size = 40960;
	static char info[info_size] = { 0 };

	av_register_all();

	sprintf_s(info, info_size, "%s\n", avcodec_configuration());

	return info;
}

int _tmain(int argc, _TCHAR* argv[])
{
	printf("\n<<Configuration>>\n%s", config_info());
	printf("\n<<URLProtocol>>\n%s", url_protocol_info());
	printf("\n<<AVFormat>>\n%s", avformat_info());
	printf("\n<<AVCodec>>\n%s", avcodec_info());
	printf("\n<<AVFilter>>\n%s", avfilter_info());

	return 0;
}
