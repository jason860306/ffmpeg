#! /bin/sh
#��򵥵�FFmpeg��AVUtilʾ�� ----MinGW�����б���
#Simplest FFmpeg AVUtil ----Compile in MinGW 
#
#������ Lei Xiaohua
#leixiaohua1020@126.com
#�й���ý��ѧ/���ֵ��Ӽ���
#Communication University of China / Digital TV Technology
#http://blog.csdn.net/leixiaohua1020
#
#compile
g++ simplest_ffmpeg_avutil.cpp -g -o simplest_ffmpeg_avutil.exe \
-I /usr/local/include -L /usr/local/lib \
-lavcodec -lavutil
