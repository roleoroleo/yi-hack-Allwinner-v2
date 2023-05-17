OBJECTS = imggrabber.o convert2jpg.o add_water.o water_mark.o
FFMPEG = ffmpeg-4.0.6
JPEGSRC = jpegsrc.v9e
FFMPEG_DIR = ./$(FFMPEG)
INC_FF = -I$(FFMPEG_DIR)
LIB_FF = $(FFMPEG_DIR)/libavcodec/libavcodec.a $(FFMPEG_DIR)/libavutil/libavutil.a -lpthread -lm
JPEGLIB = jpeg-9e
JPEGLIB_DIR = ./$(JPEGLIB)
INC_J = -I$(JPEGLIB_DIR)
LIB_J = $(JPEGLIB_DIR)/.libs/libjpeg.a
OPTS = -Os -ffunction-sections -fdata-sections

all: imggrabber

imggrabber.o: imggrabber.c $(HEADERS)
	@$(build_ffmpeg)
	@$(build_jpeglib)
	$(CC) -c $< $(OPTS) $(INC_J) $(INC_FF) -fPIC -o $@

convert2jpg.o: convert2jpg.c $(HEADERS)
	$(CC) -c $< $(OPTS) $(INC_J) -fPIC -o $@

add_water.o: add_water.c $(HEADERS)
	$(CC) -c $< $(OPTS) $(INC_J) -fPIC -o $@

water_mark.o: water_mark.c $(HEADERS)
	$(CC) -c $< $(OPTS) $(INC_J) -fPIC -o $@

imggrabber: $(OBJECTS)
	$(CC) -Os -Wl,--gc-sections $(OBJECTS) $(LIB_J) $(LIB_FF) -fPIC -o $@
	$(STRIP) $@

.PHONY: clean

clean:
	rm -f imggrabber
	rm -f $(OBJECTS)
	rm -rf $(FFMPEG)
	rm -rf $(JPEGLIB)

distclean: clean
	rm -rf SDK
	rm -rf $(JPEGLIB)

define build_ffmpeg
    # get archive
    if [ ! -f SDK/ffmpeg.tar.bz2 ]; then \
        mkdir -p SDK; \
        wget -O ./SDK/ffmpeg.tar.bz2.tmp "http://ffmpeg.org/releases/$(FFMPEG).tar.bz2"; \
        mv ./SDK/ffmpeg.tar.bz2.tmp ./SDK/ffmpeg.tar.bz2; \
    fi

    # untar
    if [ ! -f $(FFMPEG)/README.md ]; then \
         tar jxvf ./SDK/ffmpeg.tar.bz2; \
    fi

    # build
    if [ ! -f $(FFMPEG)/libavcodec/libavcodec.a ] || [ ! -f $(FFMPEG)/libavutil/libavutil.a ]; then \
         cd $(FFMPEG); \
        ./configure --enable-cross-compile --cross-prefix=$(CROSSPREFIX) --arch=armel --target-os=linux --prefix=$(CROSSPATH) --enable-small --disable-ffplay --disable-ffprobe --disable-doc  --disable-decoders --enable-decoder=h264 --enable-decoder=hevc --disable-encoders --disable-demuxers --enable-demuxer=h264 --enable-demuxer=hevc --disable-muxers --disable-protocols --disable-parsers --enable-parser=h264 --enable-parser=hevc --disable-filters --disable-bsfs --disable-indevs --disable-outdevs --extra-cflags="-Os -ffunction-sections -fdata-sections" && \
         make; \
         cd ..;\
    fi
endef

define build_jpeglib
    # get archive
    if [ ! -f SDK/jpeg.tar.gz ]; then \
        mkdir -p SDK; \
        wget -O ./SDK/jpeg.tar.gz.tmp "https://www.ijg.org/files/${JPEGSRC}.tar.gz"; \
        mv ./SDK/jpeg.tar.gz.tmp ./SDK/jpeg.tar.gz; \
    fi

    # untar
    if [ ! -f $(JPEGLIB)/README ]; then \
         tar zxvf ./SDK/jpeg.tar.gz; \
    fi

    # build
    if [ ! -f $(JPEGLIB)/.libs/libjpeg.a ]; then \
        cd $(JPEGLIB); \
        ./configure --host=$(CROSS) --enable-shared=yes && \
        make CFLAGS+="-Os -ffunction-sections -fdata-sections"; \
        cd ..;\
    fi
endef
