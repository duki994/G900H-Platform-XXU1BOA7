/*
 * Copyright 2010, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SkImageDecoder.h"
#include "SkImageEncoder.h"
#include "SkColorPriv.h"
#include "SkScaledBitmapSampler.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include "SkUtils.h"
#include "SkTRegistry.h"


#include <stdio.h>
extern "C" {
#include "maet.h"
#include "sxpi_base.h"
#include "sxqk_mtal_pthread.h"
}

#ifdef ANDROID
#include <cutils/properties.h>
#endif

static const char SPI_9PATCH_HEADER_PREFIX[] = {0xAA, 0x65, 0x00, 0x00};

#define ALIGN_16(x)                ((((x) + 15) >> 4) << 4)

#define SPI_HEADER_SIZE            4
#define CHUNK_NAME_SIZE            5
#define CHUNK_SIZE                 4
#define ENC_BUF_MIN_SIZE           1024*1000

/* define for maetel decoder function */
#define USE_CLONE                  1

/* if LOCAL_BITMAP define : use local variable in onDecodeRegion function */
/* if no LOCAL_BITMAP define : use member variable in onDecodeRegion function */
//#define LOCAL_BITMAP

/* if LOCAL_ID define : maetel instance is created when every calling onDecode function */
/* if no LOCAL_ID define : maetel instance is created when constructing class */
//#define LOCAL_ID

/* define for file dump */
//#define DEC_INPUT_DUMP
//#define DEC_OUTPUT_DUMP
//#define ENC_INPUT_DUMP
//#define ENC_OUTPUT_DUMP

#ifdef DEC_INPUT_DUMP
FILE * inputDumpFile;
#endif
#ifdef DEC_OUTPUT_DUMP
FILE * outputDumpFile;
#endif
#ifdef ENC_INPUT_DUMP
FILE * enc_inputDumpFile;
#endif
#ifdef ENC_OUTPUT_DUMP
FILE * enc_outputDumpFile;
#endif


class SkSPIImageDecoder : public SkImageDecoder {
public:
    SkSPIImageDecoder();
    virtual ~SkSPIImageDecoder();
    virtual Format getFormat() const SK_OVERRIDE {
        return kSPI_Format;
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode) SK_OVERRIDE;
#ifdef SK_BUILD_FOR_ANDROID
    virtual bool onDecodeSubset(SkBitmap* bm, const SkIRect& region) SK_OVERRIDE;
    virtual bool onBuildTileIndex(SkStreamRewindable* stream, int* width, int* height) SK_OVERRIDE;
#endif

	/* for spi codec */
protected:
	SXPI_IMGB *   decodeSPI(SkStream* stream, Mode mode);
	bool          parseSPIHeader(SkStream* stream, int* width, int* height);
	int           writeImgb(unsigned char * out_buffer, SXPI_IMGB * imgb);
	int           getImageInfo(SXPIX id, int * w_pic, int * h_pic, int * cs);
	int           setExtraConfig(SXPIX id);
	SXPI_IMGB *   allocImgb(int w, int h, int cs, bool justBounds);
	void          freeImgb(SXPI_IMGB * imgb);
	int           readNALHeader(unsigned char * buffer, int read_pos, int total_length);
	int           set9PatchInfo(unsigned char * patch_buffer, int patch_size);
	bool          is9PatchInfo(unsigned char * buffer, int total_length);

private:
    SkStream *         fInputStream;
	int                fOrgWidth;
	int                fOrgHeight;

	SkBitmap *         fRegionBitmap;

	/* for spi codec */
	SXPIX              spi_dec_id;
	SXPI_VDEC_CDSC     spi_dec_cdsc;
	SXPI_MTAL          spi_dec_mtal;

	typedef SkImageDecoder INHERITED;
};

#ifdef TIME_DECODE

#include "SkTime.h"

class AutoTimeMillis {
public:
    AutoTimeMillis(const char label[]) :
        fLabel(label) {
        if (NULL == fLabel) {
            fLabel = "";
        }
        fNow = SkTime::GetMSecs();
    }
    ~AutoTimeMillis() {
        SkDebugf("---- Time (ms): %s %d\n", fLabel, SkTime::GetMSecs() - fNow);
    }
private:
    const char* fLabel;
    SkMSec fNow;
};

#endif

SkSPIImageDecoder::SkSPIImageDecoder()
{
	int              task_cnt = 4;
	int              ret;

    fInputStream = NULL;
    fOrgWidth = fOrgHeight = 0;
    fRegionBitmap = NULL;
    fRegionBitmap = new SkBitmap;

#if !defined(LOCAL_ID)
	/* maet initialization */
	if(maet_init()) {
		SkDebugf("%s : Cannot initialize maet", __FUNCTION__);
		return;
	}

	/* maet configuration */
	memset(&spi_dec_cdsc, 0, sizeof(SXPI_VDEC_CDSC));
	spi_dec_cdsc.accel = SXPI_COD_ACCEL_NONE;

	if(task_cnt > 1) {
		ret = sxqk_mtal_init(&spi_dec_mtal, task_cnt);
		if(ret != 0) {
			SkDebugf("%s : Cannot initialize sxqk", __FUNCTION__);
			sxqk_mtal_deinit(&spi_dec_mtal);
			maet_deinit();
		}
		spi_dec_cdsc.mtal = &spi_dec_mtal;
	} else {
		spi_dec_cdsc.mtal = NULL;
	}

	spi_dec_id = maetd_create(&spi_dec_cdsc, NULL);
	if(spi_dec_id == NULL) {
		SkDebugf("%s : Cannot create maet decoder", __FUNCTION__);
		maetd_delete(spi_dec_id);
		sxqk_mtal_deinit(&spi_dec_mtal);
		maet_deinit();
		return;
	}
#endif
}


SkSPIImageDecoder::~SkSPIImageDecoder()
{
	if(fRegionBitmap)
	{
		delete fRegionBitmap;
		fRegionBitmap = NULL;
	}

	SkSafeUnref(fInputStream);

#if !defined(LOCAL_ID)
	maetd_delete(spi_dec_id);
	sxqk_mtal_deinit(&spi_dec_mtal);
	maet_deinit();
#endif
}

SXPI_IMGB * SkSPIImageDecoder::decodeSPI(SkStream* stream, Mode mode) {
	SXPIX                 id;
	SXPI_VDEC_CDSC        cdsc;
	SXPI_BITB             bitb;
	SXPI_IMGB           * imgb = NULL;
	SXPI_MTAL             mtal;
	SXPI_VDEC_STAT        stat;
	int                   bs_size, bs_read_pos;
	int                   task_cnt = 4;
	int                   w, h, w_pic, h_pic, cs, cs_out;
	int                   ret = SXPI_ERR_UNKNOWN;
	unsigned char       * bs_buf = NULL;
	const bool            justBounds = SkImageDecoder::kDecodeBounds_Mode == mode;
	/* 9 patch info */
	int                   patch_size = 0;
	int                   patch_stream_size = 0;
	unsigned char       * patch_buffer = NULL;
	bool                  is_9patch = false;
	/* Decode image to bitmap */
	size_t                read_length = 0, total_length = stream->getLength();
	SkAutoMalloc          storage(total_length);

	stream->rewind();

	read_length = stream->read((unsigned char*)storage.get(), total_length);

	if(total_length != read_length) {
		SkDebugf("%s : Cannot read the stream, get_length(%d) != read_length(%d)", __FUNCTION__, total_length, read_length);
		storage.free();
		return NULL;
	}

	/***************** Create maet decoder - start *****************/
	/* init local variable */
	w = h = w_pic = h_pic = 0;
	cs = cs_out = 0;
#if defined(LOCAL_ID)
	/* maet initialization */
	if(maet_init()) {
		SkDebugf("%s : Cannot initialize maet", __FUNCTION__);
		storage.free();
		return NULL;
	}

	/* maet configuration */
	memset(&cdsc, 0, sizeof(SXPI_VDEC_CDSC));
	cdsc.accel = SXPI_COD_ACCEL_NONE;

	w = h = w_pic = h_pic = 0;
	cs = cs_out = 0;

	if(task_cnt > 1) {
		ret = sxqk_mtal_init(&mtal, task_cnt);
		if(ret != 0) {
			SkDebugf("%s : Cannot initialize sxqk", __FUNCTION__);
			goto FINALIZE;
		}
		cdsc.mtal = &mtal;
	} else {
		cdsc.mtal = NULL;
	}

	id = maetd_create(&cdsc, NULL);
	if(id == NULL) {
		SkDebugf("%s : Cannot create maet decoder", __FUNCTION__);
		goto FINALIZE;
	}
#else
	id = spi_dec_id;
#endif
	/***************** Create maet decoder - end *****************/

	/***************** Decode SPI image - start *****************/
	bs_buf = (unsigned char*)storage.get();
	bs_size = bs_read_pos = 0;

	is_9patch = this->is9PatchInfo(bs_buf, total_length);

#if defined(DEC_INPUT_DUMP)
	inputDumpFile = NULL;
	inputDumpFile = fopen("//data//inputspi.spi", "wb");
	if(inputDumpFile) {
		fwrite(bs_buf, 1, total_length, inputDumpFile);
		fclose(inputDumpFile);
	}
#endif

	while(1) {
		/* read 4 bytes to get bs_size */
		bs_read_pos += SPI_HEADER_SIZE;
		if(bs_read_pos > total_length) {
			break;
		}
		memcpy(&bs_size, bs_buf, SPI_HEADER_SIZE);
		bs_buf += SPI_HEADER_SIZE;

		bitb.addr = bs_buf;
		bitb.size = bs_size;

		bs_buf += bs_size;
		bs_read_pos += bs_size;

		/* main decoding block */
		ret = maetd_decode(id, &bitb, &stat);
		if(SXPI_IS_ERR(ret)) {
			/* fail decoding */
			SkDebugf("%s : Decoding failed : error=%d", __FUNCTION__, ret);
			goto FINALIZE;
		}

		if(stat.btype == MAET_BT_SQH) {
			getImageInfo(id, &w_pic, &h_pic, &cs);

			if(justBounds && (is_9patch == false)) {
				/* imgb alloc */
				if(imgb) freeImgb(imgb);
				imgb = allocImgb(w_pic, h_pic, cs, justBounds);
				goto FINALIZE;
			}
		} else if(stat.btype == MAET_BT_9PATCH) {
			if(this->getPeeker()) {
				patch_size = sizeof(int);
				/* get 9 patch stream size */
				if(maetd_config(id, MAET_COD_CFG_GET_9PATCH_STREAM_SIZE,
					(void *)&patch_stream_size, &patch_size) != SXPI_OK) {
					SkDebugf("%s : Cannot get 9 patch stream size(%d)", __FUNCTION__, patch_stream_size);
					goto FINALIZE;
				}
	
				if(patch_stream_size == 0) {
					SkDebugf("%s : Patch stream size(%d) is invalid", __FUNCTION__, patch_stream_size);
					goto FINALIZE;
				}
	
				patch_buffer = (unsigned char *)malloc(patch_stream_size);
				if(patch_buffer == NULL) {
					SkDebugf("%s : Cannot allocate buffer for 9 patch", __FUNCTION__);
					goto FINALIZE;
				}
				memset(patch_buffer, 0, patch_stream_size);
	
				if(maetd_config(id, MAET_COD_CFG_GET_9PATCH_STREAM,
					(void *)patch_buffer, &patch_stream_size) != SXPI_OK) {
					SkDebugf("%s : Cannot get 9 patch stream", __FUNCTION__);
					goto FINALIZE;
				}
	
				this->set9PatchInfo(patch_buffer, patch_stream_size);
	
				if(justBounds && (is_9patch == true)) {
					/* imgb alloc */
					if(imgb) freeImgb(imgb);
					imgb = allocImgb(w_pic, h_pic, cs, justBounds);
					goto FINALIZE;
				}
			}
		}

		if(stat.read != bitb.size) {
			SkDebugf("%s : Different reading size of bitstream", __FUNCTION__);
		}

		if(stat.fnum >= 0) {
			SkScaledBitmapSampler sampler(w_pic, h_pic, getSampleSize());

			/* imgb alloc */
			if(imgb) freeImgb(imgb);

			w_pic = sampler.scaledWidth();
			h_pic = sampler.scaledHeight();
			cs_out = SXPI_CS_RGBA8888;

			imgb = allocImgb(w_pic, h_pic, cs_out, justBounds);
			if(imgb == NULL) {
				SkDebugf("%s : Cannot allocate image buffer", __FUNCTION__);
				goto FINALIZE;
			}

#if defined(USE_CLONE)
			ret = maetd_clone(id, imgb);
#else
			ret = maetd_pull(id, &imgb);
#endif

			if(ret != SXPI_OK) {
				SkDebugf("%s : Cannot clone/pull image buffer", __FUNCTION__);
				if(imgb) {
					freeImgb(imgb);
					imgb = NULL;
				}
				goto FINALIZE;
			}
		}
	}
	/***************** Decode SPI image - end *****************/

FINALIZE:
#if defined(LOCAL_ID)
	maetd_delete(id);
	sxqk_mtal_deinit(&mtal);
	maet_deinit();
#endif

	if(patch_buffer) {
		free(patch_buffer);
		patch_buffer = NULL;
	}

	storage.free();

	return imgb;
}

bool SkSPIImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
	SXPI_IMGB           * imgb = NULL;
	int                   w_pic, h_pic;
	bool                  bReturn = false, reallyHasAlpha = false;
	int                   i;
	unsigned char       * buf, * p;

	/***************** Decode SPI image - start *****************/
	const bool justBounds = SkImageDecoder::kDecodeBounds_Mode == mode;
	const SkBitmap::Config config = SkBitmap::kARGB_8888_Config;
    //SkBitmap::Config config = this->getPrefConfig(k32Bit_SrcDepth, true);

	imgb = decodeSPI(stream, mode);

	if(imgb) {
		/* In decodeSPI function, we already compute width and height used by getSampleSize()
		 * so SkScaledBitmapSampler use sample size of "1"
		 */
		const int sampleSize = 1;
		w_pic = imgb->w;
		h_pic = imgb->h;

	    if (!this->chooseFromOneChoice(config, w_pic, h_pic)) {
			SkDebugf("%s : chooseFromOneChoice failed", __FUNCTION__);
	        bReturn = false;
			goto FINALIZE;
	    }

		SkScaledBitmapSampler sampler(w_pic, h_pic, sampleSize);

		bm->setConfig(config, sampler.scaledWidth(), sampler.scaledHeight());
        //bm->setAlphaType(kPremul_SkAlphaType);

		if(justBounds) {
			bReturn = true;
			goto FINALIZE;
		} else {
			if(!this->allocPixelRef(bm, NULL)) {
				SkDebugf("%s : Cannot allocate pixel ref", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}

            SkAutoLockPixels alp(*bm);

			//if(!sampler.begin(bm, SkScaledBitmapSampler::kRGBA, getDitherImage())) {
            if(!sampler.begin(bm, SkScaledBitmapSampler::kRGBA, *this)) {
				SkDebugf("%s : Cannot begin SkScaledBitmapSampler", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}

			if(w_pic == sampler.scaledWidth()
				&& h_pic == sampler.scaledHeight()) {
				p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
				for(i = 0; i < h_pic; i++) {
					reallyHasAlpha |= sampler.next(p+imgb->x);
					p += imgb->s[0];
				}

				bm->setAlphaType( reallyHasAlpha?kPremul_SkAlphaType:kOpaque_SkAlphaType );
#if 0
				p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
				for(i = 0; i < h_pic; i++) {
					sampler.next(p+imgb->x);
					p += imgb->s[0];
				}
#endif
#ifdef DEC_OUTPUT_DUMP
                char str[100];
                static int num = 0;
                sprintf(str, "//data//outputspi%d_%dx%d.RGBA8888", num++, imgb->w, imgb->h);
                outputDumpFile = NULL;
                //outputDumpFile = fopen("//data//outputspi.RGBA8888", "wb");
                outputDumpFile = fopen(str, "wb");
                if(outputDumpFile) {
                    fwrite((unsigned char *)bm->getPixels(), 1, bm->getSize(), outputDumpFile);
#if 0
                    p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
                    for(i=0; i<imgb->h; i++)
                    {
                        //fwrite(p+imgb->x, imgb->w, 4, fp);
                        fwrite((unsigned char *)p+imgb->x, 1, imgb->s[0], outputDumpFile);
                        p += imgb->s[0];
                    }
#endif

                    fclose(outputDumpFile);
                }
#endif
				bReturn = true;
			} else {
				SkDebugf("%s : w(%d) != scaled_w(%d), h(%d) != scaled_h(%d)",
					__FUNCTION__, w_pic, sampler.scaledWidth(), h_pic, sampler.scaledHeight());
			}

#if 0
			unsigned char * out_buffer = (unsigned char*)bm->getPixels();

			if(out_buffer == NULL) {
				SkDebugf("%s : Cannot allocate output bitmap buffer", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}

			if(w_pic == sampler.scaledWidth()
				&& h_pic == sampler.scaledHeight()) {
				writeImgb(out_buffer, imgb);
				bReturn = true;
			} else {
				SkDebugf("%s : w(%d) != scaled_w(%d), h(%d) != scaled_h(%d)",
					__FUNCTION__, w_pic, sampler.scaledWidth(), h_pic, sampler.scaledHeight());
			}
#endif
		}
	}
	/***************** Decode SPI image - end *****************/

#if 0//ENCODE test code
	if(bReturn) {
		SkAutoTDelete<SkImageEncoder> enc_spi(SkImageEncoder::Create(SkImageEncoder::kSPI_Type));
		//SkWStream enc_stream;

		if(enc_spi.get() && enc_spi.get()->encodeStream(NULL, *bm, 0) == false) {
			SkDebugf("%s : encoding failed", __FUNCTION__);
		}
	}
#endif

FINALIZE:
	if(imgb) {
		freeImgb(imgb);
	}
#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG
    SkDebugf("GFX spi bitmap created width:%d height:%d bitmap id is %d \n",bm->width(),bm->height(),bm->getGenerationID());
#endif

	return bReturn;
}

bool SkSPIImageDecoder::onBuildTileIndex(SkStreamRewindable *stream, int *width, int *height)
{
	if(!parseSPIHeader(stream, width, height)) {
		SkDebugf("%s : Cannot parse header w(%d), h(%d)", __FUNCTION__, *width, *height);
		return false;
	}

    if(!stream->rewind()) {
        SkDebugf("%s : Failed to rewind spi stream!", __FUNCTION__);
    }
    SkRefCnt_SafeAssign(this->fInputStream, stream);
	this->fOrgWidth = *width;
	this->fOrgHeight = *height;

	return true;
}


bool SkSPIImageDecoder::onDecodeSubset(SkBitmap* bm, const SkIRect& region) {
	SXPI_IMGB           * imgb = NULL;
	int                   w_pic, h_pic;
	bool                  bReturn = false;
	bool                  directDecode = false;
	unsigned char *       p;
	int                   i;
	SkIRect rect = SkIRect::MakeWH(fOrgWidth, fOrgHeight);

	if(!rect.intersect(region)) {
        // If the requested region is entirely outsides the image, just
        // returns false
        return false;
	}

	const int sampleSize = this->getSampleSize();
	SkScaledBitmapSampler sampler(rect.width(), rect.height(), sampleSize);
	const int scaled_width = sampler.scaledWidth();
	const int scaled_height = sampler.scaledHeight();
	//SkBitmap::Config config = this->getPrefConfig(k32Bit_SrcDepth, false);
	SkBitmap::Config config = SkBitmap::kARGB_8888_Config;

	int startX = rect.fLeft;
	int startY = rect.fTop;
	int width = rect.width();
	int height = rect.height();
	int actualSampleSize = this->getSampleSize();

	int w = rect.width() / actualSampleSize;
	int h = rect.height() / actualSampleSize;

#if defined(LOCAL_BITMAP)
	SkBitmap * decodedBitmap = new SkBitmap;
	SkAutoTDelete<SkBitmap> adb(decodedBitmap);

	if(sampleSize > 1) {
		SkScaledBitmapSampler resampler(fOrgWidth, fOrgHeight, sampleSize);
		decodedBitmap->setConfig(config, resampler.scaledWidth(), resampler.scaledHeight());
	} else {
		decodedBitmap->setConfig(config, fOrgWidth, fOrgHeight);
	}
#else
	if(sampleSize > 1) {
		SkScaledBitmapSampler resampler(fOrgWidth, fOrgHeight, sampleSize);

		if(resampler.scaledWidth() != fRegionBitmap->width() ||
			resampler.scaledHeight() != fRegionBitmap->height()) {
			// free pixels
			fRegionBitmap->reset();
			// set new width and height
			fRegionBitmap->setConfig(config, resampler.scaledWidth(), resampler.scaledHeight());
			// allocate new pixels
			if(!fRegionBitmap->allocPixels()) {
				SkDebugf("%s : Cannot allocate region bitmap pixel", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}
		}
	} else {
		if(fOrgWidth != fRegionBitmap->width() ||
			fOrgHeight != fRegionBitmap->height()) {
			// free pixels
			fRegionBitmap->reset();
			// set new width and height
			fRegionBitmap->setConfig(config, fOrgWidth, fOrgHeight);
			// allocate new pixels
			if(!fRegionBitmap->allocPixels()) {
				SkDebugf("%s : Cannot allocate region bitmap pixel", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}
		}
	}
#endif

	// The image can be decoded directly to decodedBitmap if
    //   1. the region is within the image range
    //   2. bitmap's config is compatible
    //   3. bitmap's size is same as the required region (after sampled)
#if defined(LOCAL_BITMAP)
    directDecode = (rect == region) &&
		bm->isNull() &&
		(w == decodedBitmap->width()) && (h == decodedBitmap->height()) &&
		((startX - rect.x()) / actualSampleSize == 0) &&
		((startY - rect.y()) / actualSampleSize == 0);
#else
    directDecode = (rect == region) &&
		bm->isNull() &&
		(w == fRegionBitmap->width()) && (h == fRegionBitmap->height()) &&
		((startX - rect.x()) / actualSampleSize == 0) &&
		((startY - rect.y()) / actualSampleSize == 0);
#endif

	directDecode = false;

	/***************** Decode SPI image - start *****************/
	imgb = decodeSPI(this->fInputStream, SkImageDecoder::kDecodePixels_Mode);
	/***************** Decode SPI image - end *****************/

	if(imgb) {
		w_pic = imgb->w;
		h_pic = imgb->h;

#if defined(LOCAL_BITMAP)
		if(directDecode) {
			if(!this->allocPixelRef(decodedBitmap, NULL)) {
				SkDebugf("%s : Cannot allocate pixel ref", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}
		} else {
			if(!decodedBitmap->allocPixels()) {
				SkDebugf("%s : Cannot allocate pixel", __FUNCTION__);
				bReturn = false;
				goto FINALIZE;
			}
		}

		SkAutoLockPixels alp(*decodedBitmap);

		//if(!sampler.begin(decodedBitmap, SkScaledBitmapSampler::kRGBA, getDitherImage())) {
        if(!sampler.begin(decodedBitmap, SkScaledBitmapSampler::kRGBA, *this)) {
			SkDebugf("%s : Cannot begin sampler", __FUNCTION__);
			bReturn = false;
			goto FINALIZE;
		}

		if(w_pic == decodedBitmap->width()
			&& h_pic == decodedBitmap->height()) {

			p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
			for(i = 0; i < h_pic; i++) {
				sampler.next(p+imgb->x);
				p += imgb->s[0];
			}

			if(directDecode) {
				bm->swap(*decodedBitmap);
			} else {
//				cropBitmap(bm, decodedBitmap, sampleSize, region.x(), region.y(),
//					region.width(), region.height(), rect.x(), rect.y());
				cropBitmap(bm, decodedBitmap, sampleSize, region.x(), region.y(),
					region.width(), region.height(), 0, 0);
			}
			bm->setAlphaType(kPremul_SkAlphaType);
			bReturn = true;
		} else {
			SkDebugf("%s : sampleSize(%d), w(%d) != scaled_w(%d), h(%d) != scaled_h(%d)",
				__FUNCTION__, sampleSize, w_pic, decodedBitmap->width(), h_pic, decodedBitmap->height());
		}
#else
		SkAutoLockPixels alp(*fRegionBitmap);

		//if(!sampler.begin(fRegionBitmap, SkScaledBitmapSampler::kRGBA, getDitherImage())) {
        if(!sampler.begin(fRegionBitmap, SkScaledBitmapSampler::kRGBA, *this)) {
			SkDebugf("%s : Cannot begin sampler", __FUNCTION__);
			bReturn = false;
			goto FINALIZE;
		}

		if(w_pic == fRegionBitmap->width()
			&& h_pic == fRegionBitmap->height()) {
			p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
			for(i = 0; i < h_pic; i++) {
				sampler.next(p+imgb->x);
				p += imgb->s[0];
			}

			if(directDecode) {
				bm->swap(*fRegionBitmap);
			} else {
//				cropBitmap(bm, decodedBitmap, sampleSize, region.x(), region.y(),
//					region.width(), region.height(), rect.x(), rect.y());
				cropBitmap(bm, fRegionBitmap, sampleSize, region.x(), region.y(),
					region.width(), region.height(), 0, 0);
			}
			bm->setAlphaType(kPremul_SkAlphaType);
            fRegionBitmap->notifyPixelsChanged();
			bReturn = true;
		} else {
			SkDebugf("%s : sampleSize(%d), w(%d) != scaled_w(%d), h(%d) != scaled_h(%d)",
				__FUNCTION__, sampleSize, w_pic, fRegionBitmap->width(), h_pic, fRegionBitmap->height());
		}
#endif
	}

FINALIZE:

	if(imgb) {
		freeImgb(imgb);
	}
	return bReturn;
}


bool SkSPIImageDecoder::parseSPIHeader(SkStream *stream, int *width, int *height) {
	SXPI_IMGB           * imgb = NULL;
	bool                  bReturn = false;

	imgb = decodeSPI(stream, SkImageDecoder::kDecodeBounds_Mode);

	if(imgb) {
		*width = imgb->w;
		*height = imgb->h;
		bReturn = true;
	} else {
		bReturn = false;
	}

FINALIZE:
	if(imgb) {
		freeImgb(imgb);
	}
	return bReturn;
}



int SkSPIImageDecoder::setExtraConfig(SXPIX id)
{
#if 0
    int size, value;

	/* test MIN delay */
	value = 2;
	size = sizeof(int);
	sx264d_config(id, MAET_CFG_SET_DELAY_MIN, &value, &size);

	/* test MAX delay */
	value = 4;
	size = sizeof(int);
	sx264d_config(id, MAET_CFG_SET_DELAY_MAX, &value, &size);
#endif
    return 0;
}

int SkSPIImageDecoder::readNALHeader(unsigned char * buffer, int read_pos, int total_length) {
	int i, bs_size;

	if(read_pos + 6 > total_length)
		return 0;

	bs_size = 0;
	/* 1 byte : MARKER + 1 byte : TYPE */
	buffer += 2;
	for(i = 0; i < SPI_HEADER_SIZE; i++) {
		bs_size |= ((int)*buffer++) << (24 - (i<<3));
	}

	if(read_pos + bs_size > total_length)
		return 0;

	return bs_size;
}

int SkSPIImageDecoder::set9PatchInfo(unsigned char * patch_buffer, int patch_size)
{
	char              chunk_name[CHUNK_NAME_SIZE];
	size_t            chunk_size;
	unsigned char   * chunk_data;
	int               cur_read_pos;

	cur_read_pos = 0;

	while(cur_read_pos < patch_size) {
		/* initialize variable */
		memset(chunk_name, 0, CHUNK_NAME_SIZE);
		chunk_size = 0;
		chunk_data = NULL;

		/* get information of 9 patch */
		memcpy(chunk_name, patch_buffer+cur_read_pos, CHUNK_NAME_SIZE);
		cur_read_pos += CHUNK_NAME_SIZE;
		memcpy(&chunk_size, patch_buffer+cur_read_pos, CHUNK_SIZE);
		cur_read_pos += CHUNK_SIZE;

		chunk_data = (unsigned char *)malloc(chunk_size);
		if(chunk_data == NULL) {
			SkDebugf("%s : Cannot allocate chunk data buffer", __FUNCTION__);
			return SXPI_ERR_OUT_OF_MEMORY;
		}
		memset(chunk_data, 0, chunk_size);
		memcpy(chunk_data, patch_buffer+cur_read_pos, chunk_size);
		cur_read_pos += chunk_size;

		if(this->getPeeker()) {
			SkImageDecoder::Peeker * peeker = (SkImageDecoder::Peeker *)this->getPeeker();
			if(peeker->peek((const char*)chunk_name, chunk_data, chunk_size) != true) {
				SkDebugf("%s : peek failed chunk_name(%s), chunk_size(%d), chunk_data(%p, %p)",
					__FUNCTION__, chunk_name, chunk_size, patch_buffer, chunk_data);
			}
		}

        if(chunk_data) {
            free(chunk_data);
            chunk_data = NULL;
        }
	}

	return SXPI_OK;
}

bool SkSPIImageDecoder::is9PatchInfo(unsigned char * buffer, int total_length)
{
	int               read_length;
	int               data_size, i;
	
	read_length = 0;

	while(read_length < total_length) {
		data_size = 0;
		for(i = 0; i < SPI_HEADER_SIZE; i++) {
			data_size |= (int)buffer[i] << (i<<3);
		}
		read_length += SPI_HEADER_SIZE;
		buffer += SPI_HEADER_SIZE;

		/* check SPI HEADER whether 9 patch infomation or not */
		if(!memcmp(buffer, SPI_9PATCH_HEADER_PREFIX, sizeof(SPI_9PATCH_HEADER_PREFIX))) {
			return true;
		}

		read_length += data_size;
		buffer += data_size;
	}
	return false;
}


int SkSPIImageDecoder::getImageInfo(SXPIX id, int * w_pic, int * h_pic, int * cs)
{
	int size;

	size = sizeof(int);
	maetd_config(id, SXPI_COD_CFG_GET_WIDTH, (void *)w_pic, &size);
	size = sizeof(int);
	maetd_config(id, SXPI_COD_CFG_GET_HEIGHT, (void *)h_pic, &size);
	size = sizeof(int);
	maetd_config(id, SXPI_COD_CFG_GET_COLOR_SPACE, (void *)cs, &size);

	return SXPI_OK;
}

SXPI_IMGB * SkSPIImageDecoder::allocImgb(int w, int h, int cs, bool justBounds)
{
	SXPI_IMGB * imgb = NULL;

	imgb = (SXPI_IMGB *)malloc(sizeof(SXPI_IMGB));
	if(imgb == NULL)
	{
		SkDebugf("%s : Cannot create image buffer", __FUNCTION__);
		return NULL;
	}
	memset(imgb, 0, sizeof(SXPI_IMGB));
	imgb->w = w;
	imgb->h = h;
	imgb->cs = cs;

	if(imgb->cs == SXPI_CS_YUV444)
	{
		imgb->s[0] = imgb->s[1] = imgb->s[2] = imgb->w;
		imgb->e[0] = imgb->e[1] = imgb->e[2] = imgb->h;
		if(!justBounds) {
			imgb->a[0] = malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
			imgb->a[1] = malloc(imgb->s[1]*imgb->e[1]*sizeof(unsigned char));
			imgb->a[2] = malloc(imgb->s[2]*imgb->e[2]*sizeof(unsigned char));
		}
	}
	else if(imgb->cs == SXPI_CS_YUV444A8)
	{
		imgb->s[0] = imgb->s[1] = imgb->s[2] = imgb->s[3] = imgb->w;
		imgb->e[0] = imgb->e[1] = imgb->e[2] = imgb->e[3] = imgb->h;
		if(!justBounds) {
			imgb->a[0] = malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
			imgb->a[1] = malloc(imgb->s[1]*imgb->e[1]*sizeof(unsigned char));
			imgb->a[2] = malloc(imgb->s[2]*imgb->e[2]*sizeof(unsigned char));
			imgb->a[3] = malloc(imgb->s[2]*imgb->e[2]*sizeof(unsigned char));
		}
	}
	else if(SXPI_CS_IS_RGB16_PACK(cs))
	{
		imgb->s[0] = imgb->w*2;
		imgb->e[0] = imgb->h;
		if(!justBounds) {
			imgb->a[0] = malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
		}
	}
	else if(SXPI_CS_IS_RGB24_PACK(cs))
	{
		imgb->s[0] = imgb->w*3;
		imgb->e[0] = imgb->h;
		if(!justBounds) {
			imgb->a[0] = malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
		}
	}
	else if(SXPI_CS_IS_RGB32_PACK(cs))
	{
		imgb->s[0] = imgb->w*4;
		imgb->e[0] = imgb->h;
		if(!justBounds) {
			imgb->a[0] = malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
		}
	}
	else
	{
		SkDebugf("%s : unknown color space", __FUNCTION__);
		if(imgb) free(imgb);
		return NULL;
	}

	return imgb;
}

void SkSPIImageDecoder::freeImgb(SXPI_IMGB * imgb)
{
	if(imgb)
	{
		if(imgb->a[0]) free(imgb->a[0]);
		if(imgb->a[1]) free(imgb->a[1]);
		if(imgb->a[2]) free(imgb->a[2]);
		if(imgb->a[3]) free(imgb->a[3]);
		free(imgb);
	}
}

int SkSPIImageDecoder::writeImgb(unsigned char * out_buffer, SXPI_IMGB * imgb)
{
	unsigned char * p;
	int             i, j;

	if(imgb->cs == SXPI_CS_YUV444)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.YUV444", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 1, imgb->s[0]*imgb->h, outputDumpFile);
			fwrite((unsigned char *)imgb->a[1], 1, imgb->s[1]*imgb->h, outputDumpFile);
			fwrite((unsigned char *)imgb->a[2], 1, imgb->s[2]*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		/* crop image supported */
		/* luma */
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 1, fp);
			memcpy(out_buffer, p+imgb->x, imgb->w);
			out_buffer += imgb->w;
			p += imgb->s[0];
		}

		/* chroma */
		for(i=1; i<3; i++)
		{
			p = (unsigned char *)imgb->a[i] + (imgb->s[i]*imgb->y);
			for(j=0; j<imgb->h; j++)
			{
				//fwrite(p+imgb->x, imgb->w, 1, fp);
				memcpy(out_buffer, p+imgb->x, imgb->w);
				out_buffer += imgb->w;
				p += imgb->s[i];
			}
		}
	}
	else if(imgb->cs == SXPI_CS_YUV444A8)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.YUV444A8", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 1, imgb->s[0]*imgb->h, outputDumpFile);
			fwrite((unsigned char *)imgb->a[1], 1, imgb->s[1]*imgb->h, outputDumpFile);
			fwrite((unsigned char *)imgb->a[2], 1, imgb->s[2]*imgb->h, outputDumpFile);
			fwrite((unsigned char *)imgb->a[3], 1, imgb->s[3]*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		/* crop image supported */
		/* luma */
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 1, fp);
			memcpy(out_buffer, p+imgb->x, imgb->w);
			out_buffer += imgb->w;
			p += imgb->s[0];
		}

		/* chroma */
		for(i=1; i<3; i++)
		{
			p = (unsigned char *)imgb->a[i] + (imgb->s[i]*imgb->y);
			for(j=0; j<imgb->h; j++)
			{
				//fwrite(p+imgb->x, imgb->w, 1, fp);
				memcpy(out_buffer, p+imgb->x, imgb->w);
				out_buffer += imgb->w;
				p += imgb->s[i];
			}
		}

		/* alpha */
		p = (unsigned char *)imgb->a[3] + (imgb->s[3]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 1, fp);
			memcpy(out_buffer, p+imgb->x, imgb->w);
			out_buffer += imgb->w;
			p += imgb->s[3];
		}
	}
	else if(imgb->cs == SXPI_CS_RGB888)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.RGB888", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 3, imgb->w*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 3, fp);
			memcpy(out_buffer, p+imgb->x, 3*imgb->w);
			out_buffer += 3*imgb->w;
			p += imgb->s[0];
		}
	}
	else if(imgb->cs == SXPI_CS_BGR888)
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 3, fp);
			memcpy(out_buffer, p+imgb->x, 3*imgb->w);
			out_buffer += 3*imgb->w;
			p += imgb->s[0];
		}
	}
	else if(imgb->cs == SXPI_CS_RGBA8888)
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
#ifdef DEC_OUTPUT_DUMP
		char str[100];
		static int num = 0;
		sprintf(str, "//data//outputspi%d.RGBA8888", num++);
		outputDumpFile = NULL;
		//outputDumpFile = fopen("//data//outputspi.RGBA8888", "wb");
		outputDumpFile = fopen(str, "wb");
		if(outputDumpFile) {
			p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
			for(j=0; j<imgb->h; j++)
			{
				//fwrite(p+imgb->x, imgb->w, 4, fp);
				fwrite((unsigned char *)p+imgb->x, 4, imgb->w, outputDumpFile);
				p += imgb->s[0];
			}

			fclose(outputDumpFile);
		}
#endif
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 4, fp);
			memcpy(out_buffer, p+imgb->x, 4*imgb->w);
			out_buffer += 4*imgb->w;
			p += imgb->s[0];
		}
	}
	else if(imgb->cs == SXPI_CS_BGRA8888)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.yuv", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 4, imgb->w*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 4, fp);
			memcpy(out_buffer, p+imgb->x, 4*imgb->w);
			out_buffer += 4*imgb->w;
			p += imgb->s[0];
		}

	}
	else if(imgb->cs == SXPI_CS_ARGB8888)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.yuv", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 4, imgb->w*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 4, fp);
			memcpy(out_buffer, p+imgb->x, 4*imgb->w);
			out_buffer += 4*imgb->w;
			p += imgb->s[0];
		}
	}
	else if(imgb->cs == SXPI_CS_ABGR8888)
	{
#ifdef DEC_OUTPUT_DUMP
		outputDumpFile = NULL;
		outputDumpFile = fopen("//data//outputspi.yuv", "wb");
		if(outputDumpFile) {
			fwrite((unsigned char *)imgb->a[0], 4, imgb->w*imgb->h, outputDumpFile);
			fclose(outputDumpFile);
		}
#endif
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<imgb->h; j++)
		{
			//fwrite(p+imgb->x, imgb->w, 4, fp);
			memcpy(out_buffer, p+imgb->x, 4*imgb->w);
			out_buffer += 4*imgb->w;
			p += imgb->s[0];
		}
	}
	else
	{
		SkDebugf("%s : Cannot support the color space", __FUNCTION__);
		return -1;
	}
	return 0;
}



class SkSPIImageEncoder : public SkImageEncoder {
public:
	SkSPIImageEncoder();

	virtual ~SkSPIImageEncoder();

protected:
	virtual bool onEncode(SkWStream * stream, const SkBitmap& bm, int quality);

	/* for spi codec */
protected:
	int          setExtraConfig(SXPIX id);
	int          allocImgb(int w, int h, int cs, SXPI_IMGB * imgb);
	void         freeImgb(SXPI_IMGB * imgb);
	int          readImgb(unsigned char * org_buf, SXPI_IMGB * imgb);

private:
    typedef SkImageEncoder INHERITED;
};



SkSPIImageEncoder::SkSPIImageEncoder()
{
}

SkSPIImageEncoder::~SkSPIImageEncoder()
{
}

bool SkSPIImageEncoder::onEncode(SkWStream * stream, const SkBitmap& bm, int quality) {
	/* parameters for spi encoder */
	SXPIX                 id;
	SXPI_VENC_CDSC        cdsc;
	SXPI_BITB             bitb;
	SXPI_IMGB             imgb;
	SXPI_VENC_STAT        stat;
	int                   task_cnt = 1;
	int                   ret;
	SXPI_MTAL             mtal;
	double                bitrate;
	bool                  hasAlpha;
	unsigned char *       bitmap_buf = NULL;
	int                   bitmap_buf_size = 0;
	int                   value = 0, size = 0;
	unsigned char *       leng = NULL;
	unsigned char *       enc_bs_buf = NULL;
	int                   enc_bs_buf_size = 0;
	/* parameters for local functions */
	bool                  bReturn = false;

	SkAutoLockPixels      alp(bm);

	if(bm.getPixels() == NULL) {
		SkDebugf("%s : bm.getPixels() is NULL");
		bReturn = false;
		return bReturn;
	}

	switch(bm.config()) {
		case SkBitmap::kARGB_8888_Config:
			break;
		default: {
			SkDebugf("%s : Cannot support color format(%d)", bm.config());
			bReturn = false;
			return bReturn;
			}
	}

	hasAlpha = !bm.isOpaque();

	/*********************** INITIALIZE PARAMETERS **********************/
	memset(&cdsc, 0, sizeof(SXPI_VENC_CDSC));
	memset(&bitb, 0, sizeof(SXPI_BITB));
	memset(&imgb, 0, sizeof(SXPI_IMGB));
	memset(&stat, 0, sizeof(SXPI_VENC_STAT));
	memset(&mtal, 0, sizeof(SXPI_MTAL));

	/*********************** SET PARAMETERS *****************************/
	cdsc.w        = bm.width();
	cdsc.h        = bm.height();
	//cdsc.qp       = 0; // 0 : lossless,  qp > 0 : lossy
	cdsc.qp       = (quality == 0) ? 0 : 12; // 0 : lossless,  qp > 0 : lossy
	cdsc.rc_type  = 0;
	cdsc.bps      = 384000;
	cdsc.fps      = 30;
	cdsc.i_period = 1;
	cdsc.accel    = 1;
	cdsc.cs       = SXPI_CS_RGBA8888;

	/*********************** INITIALIZE ENCODER FUNCTION ****************/
	if(maet_init() != SXPI_OK) {
		SkDebugf("%s : Cannot initialize maet", __FUNCTION__);
		return false;
	}

	if(task_cnt > 1) {
		ret = sxqk_mtal_init(&mtal, task_cnt);
		cdsc.mtal = &mtal;
	} else {
		cdsc.mtal = NULL;
	}

	id = maete_create(&cdsc, NULL);
	if(id == NULL) {
		SkDebugf("%s : Cannot create maet encoder", __FUNCTION__);
		goto FINALIZE;
	}

	if(setExtraConfig(id) != SXPI_OK) {
		SkDebugf("%s : Cannot configure maet encoder", __FUNCTION__);
		goto FINALIZE;
	}

	enc_bs_buf_size = ALIGN_16(cdsc.w) * cdsc.h * 4;
	enc_bs_buf_size = (enc_bs_buf_size > ENC_BUF_MIN_SIZE)
						? enc_bs_buf_size : ENC_BUF_MIN_SIZE;
	enc_bs_buf = (unsigned char*)malloc(enc_bs_buf_size);
	if(enc_bs_buf == NULL) {
		SkDebugf("%s : Cannot allocate encode buffer", __FUNCTION__);
		goto FINALIZE;
	}
	memset(enc_bs_buf, 0, enc_bs_buf_size);

	bitrate = 0;
	bitb.addr = enc_bs_buf;
	bitb.size = enc_bs_buf_size;

	bitmap_buf = (unsigned char *)bm.getPixels();
	bitmap_buf_size = bm.getSize();

	/*********************** ENCODE PICTURE **************************/
	/***************** SET COMPLEXITY ********************************/
	size = sizeof(int);
	value = SXPI_COD_CPX_MAXIMUM;
	if(maete_config(id, SXPI_COD_CFG_SET_COMPLEXITY,
		(void *)&value, &size) != SXPI_OK) {
		printf("%s : Cannot configure SXPI_COD_CFG_SET_COMPLEXITY\n", __FUNCTION__);
	}

	/***************** SET QP ****************************************/
	size = sizeof(int);
	if(maete_config(id, SXPI_COD_CFG_SET_QP,
		(void *)(&cdsc.qp), &size) != SXPI_OK) {
		printf("%s : Cannot configure SXPI_COD_CFG_SET_QP\n", __FUNCTION__);
	}

	/***************** SET VLC MODE **********************************/
	value = 1; //use SBAC
	size = sizeof(int);
	if(maete_config(id, MAET_COD_CFG_SET_BAC_ENABLED,
		(void *)&value, &size) != SXPI_OK) {
		printf("%s : Cannot configure MAET_COD_CFG_SET_SBAC_ENABLED\n", __FUNCTION__);
	}


	freeImgb(&imgb);
	ret = allocImgb(cdsc.w, cdsc.h, cdsc.cs, &imgb);
	if(ret != SXPI_OK) {
		SkDebugf("%s : Cannot allocate image buffer", __FUNCTION__);
		goto FINALIZE;
	}

	if(readImgb(bitmap_buf, &imgb)) {
		SkDebugf("%s : Cannot read imgb", __FUNCTION__);
		goto FINALIZE;
	}

	ret = maete_push(id, &imgb);
	if(SXPI_IS_ERR(ret)) {
		SkDebugf("%s : maete_push failed", __FUNCTION__);
		goto FINALIZE;
	}

	/* encode sequence header */
	ret = maete_encode_header(id, &bitb, &stat);
	if(SXPI_IS_ERR(ret)) {
		SkDebugf("%s : Cannot encode header(%d)", __FUNCTION__, ret);
		goto FINALIZE;
	}

	/* store sequence header bitstream */
	if(stream && stat.write > 0) {
		leng = (unsigned char *)&stat.write;
		if(stream->write(leng, 4) == false) {
			SkDebugf("%s : Cannot write bitstream of header size", __FUNCTION__);
			goto FINALIZE;
		}
		if(stream->write(enc_bs_buf, stat.write) == false) {
			SkDebugf("%s : Cannot write bitstream", __FUNCTION__);
			goto FINALIZE;
		}
	}

#if defined(ENC_OUTPUT_DUMP)
	if(stat.write > 0) {
		enc_outputDumpFile = NULL;
		enc_outputDumpFile = fopen("//data//enc_outputspi.spi", "wb");
		if(enc_outputDumpFile) {
			fwrite(leng, 1, 4, enc_outputDumpFile);
			fwrite(enc_bs_buf, 1, stat.write, enc_outputDumpFile);
		}
	}
#endif

	ret = maete_encode(id, &bitb, &stat);
	if(SXPI_IS_ERR(ret)) {
		SkDebugf("%s : maete_encode failed(%d)", __FUNCTION__, ret);
		goto FINALIZE;
	}

	if(stream && stat.write > 0) {
#if 0 /* Slice-based */
		int slice_cnt = 0;
		unsigned char * buf_slice = enc_bs_buf;
		while(1) {
			unsigned char * leng =
				(unsigned char *)&stat.slice_size[slice_cnt];
			if(stream->write(leng, 4) == false) {
				SkDebugf("%s : Cannot write bitstream of header size", __FUNCTION__);
				goto FINALIZE;
			}
			if(stream->write(buf_slice, stat.slice_size[slice_cnt])) {
				SkDebugf("%s : Cannot write bitstream", __FUNCTION__);
				goto FINALIZE;
			}
			buf_slice += stat.slice_size[slice_cnt];
			slice_cnt++;
			if(slice_cnt >= stat.slice_cnt) { break; }
		}
#else /* Picture-based */
		leng = (unsigned char *)&stat.write;
		if(stream->write(leng, 4) == false) {
			SkDebugf("%s : Cannot write bitstream of header size", __FUNCTION__);
			goto FINALIZE;
		}
		if(stream->write(enc_bs_buf, stat.write) == false) {
			SkDebugf("%s : Cannot write bitstream", __FUNCTION__);
			goto FINALIZE;
		}
#endif
		bReturn = true;
	}

#if defined(ENC_OUTPUT_DUMP)
	if(stat.write > 0) {
		if(enc_outputDumpFile) {
			fwrite(leng, 1, 4, enc_outputDumpFile);
			fwrite(enc_bs_buf, 1, stat.write, enc_outputDumpFile);
		}
	}
#endif
FINALIZE:
#if defined(ENC_OUTPUT_DUMP)
	if(enc_outputDumpFile) {
		fclose(enc_outputDumpFile);
		enc_outputDumpFile = NULL;
	}
#endif
	maete_delete(id);
	sxqk_mtal_deinit(&mtal);

	maet_deinit();

	return bReturn;
}

int SkSPIImageEncoder::setExtraConfig(SXPIX id)
{
#if 0
	{
	    int   ret, size, value;

	    size = sizeof(int);
	    value = VAL_USE_DEBLOCK;
	    maete_config(id, MAET_CFG_SET_USE_DEBLOCK, (void *)(&value), &size);

	    value = VAL_QP_MAX;
	    maete_config(id, MAET_CFG_SET_QP_MAX, (void *)(&value), &size);

	    value = VAL_QP_MIN;
	    maete_config(id, MAET_CFG_SET_QP_MIN, (void *)(&value), &size);

	    value = MAET_RC_BUS_ROW;
	    maete_config(id, MAET_CFG_SET_BU_SIZE, (void *)(&value), &size);
	}
#endif

#if 0 /* Use MAXIMUM MODE (Default: MEDIUM MODE) */
	{
	    int   ret, size, value;

		value = SXPI_COD_CPX_MAXIMUM;
		size = sizeof(int);
	    ret = maete_config(id, SXPI_COD_CFG_SET_COMPLEXITY, &value, &size);
		if(ret < 0)
		{
			print_log("maete_config error\n");
			return ret;
		}
	}
#endif

#if 0 /* Use HIGH MODE (Default: MEDIUM MODE) */
	{
		int   ret, size, value;

		value = SXPI_COD_CPX_HIGH;
		size = sizeof(int);
		ret = maete_config(id, SXPI_COD_CFG_SET_COMPLEXITY, &value, &size);
		if(ret < 0)
		{
			print_log("maete_config error\n");
			return ret;
		}
	}
#endif

#if 0 /* Use LOW MODE (Default: MEDIUM MODE) */
	{
		int   ret, size, value;

		value = SXPI_COD_CPX_LOW;
		size = sizeof(int);
		ret = maete_config(id, SXPI_COD_CFG_SET_COMPLEXITY, &value, &size);
		if(ret < 0)
		{
			print_log("maete_config error\n");
			return ret;
		}
	}
#endif

#if 0 /* Force Natural MODE (Default: OFF) */
		{
			int   ret, size, value;

			value = 1;
			size = sizeof(int);
			ret = maete_config(id, MAET_COD_CFG_SET_FORCE_NATURAL, &value, &size);
			if(ret < 0)
			{
				print_log("maete_config error\n");
				return ret;
			}
		}
#endif
    return SXPI_OK;
}

int SkSPIImageEncoder::allocImgb(int w, int h, int cs, SXPI_IMGB * imgb)
{
	memset(imgb, 0, sizeof(SXPI_IMGB));

	imgb->w = w;
	imgb->h = h;
	imgb->cs = cs;

	if(imgb->cs == SXPI_CS_YUV444)
	{
		imgb->s[0] = imgb->s[1] = imgb->s[2] = ALIGN_16(imgb->w);
		imgb->e[0] = imgb->e[1] = imgb->e[2] = ALIGN_16(imgb->h);
		imgb->a[0] = (unsigned char*) malloc(imgb->s[0]*imgb->e[0]);
		imgb->a[1] = (unsigned char*) malloc(imgb->s[1]*imgb->e[1]);
		imgb->a[2] = (unsigned char*) malloc(imgb->s[2]*imgb->e[2]);
		if(!imgb->a[0] || !imgb->a[1] || !imgb->a[2]) {
			SkDebugf("%s : Cannot allocate imgb(cs : %d) buffer", __FUNCTION__, imgb->cs);
			return SXPI_ERR_OUT_OF_MEMORY;
		}
		memset(imgb->a[0], 0, imgb->s[0]*imgb->e[0]);
		memset(imgb->a[1], 0, imgb->s[1]*imgb->e[1]);
		memset(imgb->a[2], 0, imgb->s[2]*imgb->e[2]);
	}
	else if(imgb->cs == SXPI_CS_YUV444A8)
	{
		imgb->s[0] = imgb->s[1] = imgb->s[2] = imgb->s[3] = ALIGN_16(imgb->w);
		imgb->e[0] = imgb->e[1] = imgb->e[2] = imgb->e[3] = ALIGN_16(imgb->h);
		imgb->a[0] = (unsigned char*) malloc(imgb->s[0]*imgb->e[0]*sizeof(unsigned char));
		imgb->a[1] = (unsigned char*) malloc(imgb->s[1]*imgb->e[1]*sizeof(unsigned char));
		imgb->a[2] = (unsigned char*) malloc(imgb->s[2]*imgb->e[2]*sizeof(unsigned char));
		imgb->a[3] = (unsigned char*) malloc(imgb->s[2]*imgb->e[2]*sizeof(unsigned char));
		if(!imgb->a[0] || !imgb->a[1] || !imgb->a[2] || !imgb->a[3]) {
			SkDebugf("%s : Cannot allocate imgb(cs : %d) buffer", __FUNCTION__, imgb->cs);
			return SXPI_ERR_OUT_OF_MEMORY;
		}
		memset(imgb->a[0], 0, imgb->s[0]*imgb->e[0]);
		memset(imgb->a[1], 0, imgb->s[1]*imgb->e[1]);
		memset(imgb->a[2], 0, imgb->s[2]*imgb->e[2]);
		memset(imgb->a[3], 0, imgb->s[3]*imgb->e[3]);
	}
	else if(SXPI_CS_IS_RGB24_PACK(cs))
	{
		imgb->s[0] = ALIGN_16(imgb->w)*3;
		imgb->e[0] = ALIGN_16(imgb->h);
		imgb->a[0] = (unsigned char*) malloc(imgb->s[0]*imgb->e[0]);
		if(!imgb->a[0]) {
			SkDebugf("%s : Cannot allocate imgb(cs : %d) buffer", __FUNCTION__, imgb->cs);
			return SXPI_ERR_OUT_OF_MEMORY;
		}
		memset(imgb->a[0], 0, imgb->s[0]*imgb->e[0]);
	}
	else if(SXPI_CS_IS_RGB32_PACK(cs))
	{
		imgb->s[0] = ALIGN_16(imgb->w)*4;
		imgb->e[0] = ALIGN_16(imgb->h);
		imgb->a[0] = (unsigned char*) malloc(imgb->s[0]*imgb->e[0]);
		if(!imgb->a[0]) {
			SkDebugf("%s : Cannot allocate imgb(cs : %d) buffer", __FUNCTION__, imgb->cs);
			return SXPI_ERR_OUT_OF_MEMORY;
		}
		memset(imgb->a[0], 0, imgb->s[0]*imgb->e[0]);
	}
	else
	{
		SkDebugf("%s : unknown color space", __FUNCTION__);
		return SXPI_ERR_UNSUPPORTED_CS;
	}

	return SXPI_OK;
}

void SkSPIImageEncoder::freeImgb(SXPI_IMGB * imgb)
{
	if(imgb)
	{
		if(imgb->a[0]) free(imgb->a[0]);
		if(imgb->a[1]) free(imgb->a[1]);
		if(imgb->a[2]) free(imgb->a[2]);
		if(imgb->a[3]) free(imgb->a[3]);
		memset(imgb, 0, sizeof(SXPI_IMGB));
	}
}

int SkSPIImageEncoder::readImgb(unsigned char * org_buf, SXPI_IMGB * imgb)
{
	unsigned char * p;
	int j;
	int f_w, f_h;

	f_w = imgb->w;
	f_h = imgb->h;

	if(imgb->cs == SXPI_CS_YUV444)
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[0];
		}
		p = (unsigned char *)imgb->a[1] + (imgb->s[1]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[1];
		}
		p = (unsigned char *)imgb->a[2] + (imgb->s[2]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[2];
		}
	}
	else if(imgb->cs == SXPI_CS_YUV444A8)
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[0];
		}
		p = (unsigned char *)imgb->a[1] + (imgb->s[1]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[1];
		}
		p = (unsigned char *)imgb->a[2] + (imgb->s[2]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[2];
		}
		p = (unsigned char *)imgb->a[3] + (imgb->s[3]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, f_w);
			org_buf += f_w;
			p += imgb->s[3];
		}
	}
	else if(SXPI_CS_IS_RGB24_PACK(imgb->cs))
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, 3*f_w);
			org_buf += 3*f_w;
			p += imgb->s[0];
		}
	}
	else if(SXPI_CS_IS_RGB32_PACK(imgb->cs))
	{
		p = (unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y);
		for(j=0; j<f_h; j++)
		{
			memcpy(p+imgb->x, org_buf, 4*f_w);
			org_buf += 4*f_w;
			p += imgb->s[0];
		}
#if defined(ENC_INPUT_DUMP)
		SkDebugf("%s : imgb->s[0](%d), imgb->h(%d), imgb->y(%d)",
			__FUNCTION__, imgb->s[0], imgb->h, imgb->y);
		enc_inputDumpFile = NULL;
		enc_inputDumpFile = fopen("//data//enc_inputspi.RGBA8888", "wb");
		if(enc_inputDumpFile) {
			fwrite((unsigned char *)imgb->a[0] + (imgb->s[0]*imgb->y), 1, imgb->s[0]*imgb->h, enc_inputDumpFile);
			fclose(enc_inputDumpFile);
		}
#endif
	}
	else
	{
		SkDebugf("%s : not supported color space(%d)", __FUNCTION__, imgb->cs);
		return SXPI_ERR_UNSUPPORTED;
	}

	return SXPI_OK;
}


///////////////////////////////////////////////////////////////////////////////
DEFINE_DECODER_CREATOR(SPIImageDecoder);
DEFINE_ENCODER_CREATOR(SPIImageEncoder);
///////////////////////////////////////////////////////////////////////////////

static bool is_spi(SkStreamRewindable* stream) {
    static const char kSPIMagic[] = { 0xAA, 0x01 };

	size_t len = stream->getLength();
    char buffer[sizeof(kSPIMagic)];

    if(len > sizeof(kSPIMagic)
        && (stream->skip(4) == 4) /* the first 4 bytes represents buffer size in spi codec */
        && stream->read(buffer, sizeof(kSPIMagic)) == sizeof(kSPIMagic) /* next 2 bytes is the prefix to spi codec */
        && !memcmp(buffer, kSPIMagic, sizeof(kSPIMagic))) {
            return true;
    }
    return false;
}

SkImageDecoder* sk_libspi_dfactory(SkStreamRewindable* stream) {
    if (is_spi(stream)) {
        return SkNEW(SkSPIImageDecoder);
    }
    return NULL;
}

static SkImageDecoder::Format get_format_spi(SkStreamRewindable* stream) {
    if (is_spi(stream)) {
        return SkImageDecoder::kSPI_Format;
	}
    return SkImageDecoder::kUnknown_Format;
}

static SkImageEncoder* sk_libspi_efactory(SkImageEncoder::Type t) {
	return (SkImageEncoder::kSPI_Type == t) ? SkNEW(SkSPIImageEncoder) : NULL;
}

static SkImageDecoder_DecodeReg gDReg(sk_libspi_dfactory);
static SkImageDecoder_FormatReg gFormatReg(get_format_spi);
static SkImageEncoder_EncodeReg gEReg(sk_libspi_efactory);


