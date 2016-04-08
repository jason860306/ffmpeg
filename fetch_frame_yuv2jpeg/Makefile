FFMPEG_BASE=/opt/Documents/bfp2p/p2p_server/branches/nasa/server/thirdparty/ffmpeg
FFMPEG_HOME=$(FFMPEG_BASE)/ffmpeg-2.4.5/linux
LAME_HOME=${FFMPEG_BASE}/lame-3.99.5/linux
VOAACENC_HOME=${FFMPEG_BASE}/vo-aacenc-0.1.3/linux
X264_HOME=${FFMPEG_BASE}/x264-snapshot-20141027-2245/linux

INCLUDE=-I$(LAME_HOME)/include -I$(VOAACENC_HOME)/include -I$(X264_HOME)/include -I$(FFMPEG_HOME)/include
LIBS=-L${FFMPEG_HOME}/lib -L${LAME_HOME}/lib -L${VOAACENC_HOME}/lib -L${X264_HOME}/lib \
    -Wl,-Bstatic -lavformat -lavcodec -lswscale -lswresample -lavutil -lmp3lame -lvo-aacenc \
    -lx264 -Wl,-Bdynamic -lz -lpthread
MACRO=-D__STDC_CONSTANT_MACROS
CFLAGS=-g -Wall -O0 ${MACRO}
#-gstabs+

BINARY=$(patsubst %.cpp,%,$(wildcard *.cpp))
OBJECT=$(BINARY:%=%.o)
BIN=fetch_jpeg

.PHONE: all clean install test
all:$ $(BIN)

$(BIN):$(OBJECT)
	@echo compiling $@
	$(CXX) $^ -o $@ $(INCLUDE) $(LIBS) $(CFLAGS)

$(OBJECT): $(@:%.o=%.cpp)
	@echo compiling $(@:%.o=%.cpp)
	$(CXX) -c $(@:%.o=%.cpp) $(INCLUDE) $(CFLAGS)

clean:
	@rm -rf ${BINARY}
	@rm -rf $(OBJECT)
	@rm -rf $(BIN)
	@rm -rf *~

install:
	@cp *.h ${SERVER_ROOT}/include

test:
	@echo "BIN: " $(BIN)
	@echo "BINARY: " $(BINARY)
	@echo "OBJECT: " $(OBJECT)
