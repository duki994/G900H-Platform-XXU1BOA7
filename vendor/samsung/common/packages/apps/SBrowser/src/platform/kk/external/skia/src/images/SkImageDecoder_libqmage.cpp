//#define LOG_NDEBUG 0

#define LOG_TAG "Qmage"
#include <utils/Log.h>

#include "SkImageDecoder.h"
#include "SkScaledBitmapSampler.h"
#include "SkStream.h"
#include "SkColorPriv.h"
#include "SkTDArray.h"
#include "SkTRegistry.h"
#include "SkStreamHelpers.h"

#include "QmageDecoder.h"


//#define PRINT_LOG_FOR_DEBUG		// 120613 Cheus
//#define PRINT_LOG_FOR_DEBUG_FUNC 
#ifdef LOG_NDEBUG
#if LOG_NDEBUG
#define PRINT_HEADER(x) printHeaderInfo(x)
#else
#define PRINT_HEADER(x)
#endif
#else
#define PRINT_HEADER(x)
#endif

#define MINIMUM_HEADER_SIZE 16

typedef struct rgbConfig_ {
	int pixelSize;
	bool isOpaque;
	SkScaledBitmapSampler::SrcConfig samplerSc;
	SkBitmap::Config bitmapSc;
}rgbConfig;

class SkQmageImageIndex {
public:
	SkQmageImageIndex(void* qmage_region_info, unsigned int width, unsigned int height)
	{
		mQmageRegionInfo = qmage_region_info;
		mWidth = width;
		mHeight = height;
	}

	~SkQmageImageIndex()
	{
		QuramQmageDestroyRegionInfo(mQmageRegionInfo);
	}

	void* mQmageRegionInfo;
	unsigned int mWidth;
	unsigned int mHeight;
};

class SkQmageImageDecoder : public SkImageDecoder
{
public:
	SkQmageImageDecoder()
	{
		fImageIndex = NULL;
	}

	virtual Format getFormat() const
	{
		return kQMG_Format;
	}

	virtual ~SkQmageImageDecoder()
	{
		SkDELETE(fImageIndex);
	}

protected:
#if defined(P1_QMAGE_IMGCODEC)
	virtual bool onQmageDecode(SkStream* stream, SkBitmap* bm,
		SkBitmap::Config pref, Mode);
#endif

	virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode);
	// for support region decoder
	virtual bool onBuildTileIndex(SkStreamRewindable *stream, int *width, int *height) SK_OVERRIDE;
	virtual bool onDecodeSubset(SkBitmap* bitmap, const SkIRect& region) SK_OVERRIDE;

private:
	SkQmageImageIndex* fImageIndex;

	typedef SkImageDecoder INHERITED;
};

static int sk_read_np_chunk(void *peeker_ptr, QuramQmageNinePatchedChunk* chunk_ptr) {
	SkImageDecoder::Peeker* peeker = (SkImageDecoder::Peeker*)peeker_ptr;
	// peek() returning true means continue decoding
	//ALOGE("sk_read_np_chunk");
	return peeker->peek((const char*)chunk_ptr->name, chunk_ptr->data_ptr, chunk_ptr->size) ? 1 : -1;
}


static bool sk_canUpscalePaletteToConfig(SkBitmap::Config prefConfig, bool srcHasAlpha) {
	switch (prefConfig) {
	case SkBitmap::kARGB_8888_Config:
	case SkBitmap::kARGB_4444_Config:
		return true;
	case SkBitmap::kRGB_565_Config:
		// only return true if the src is opaque (since 565 is opaque)
		return !srcHasAlpha;
	default:
		return false;
	}
}

#if defined(P1_QMAGE_IMGCODEC)
bool SkQmageImageDecoder::onQuramDecode(SkStream* stream, SkBitmap* bm,
	SkBitmap::Config pref, Mode mode) {
	//ALOGE("onQuramDecode : return false");
	return false;
}
#endif

bool SkQmageImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {

	//	size_t              length = 0;
	size_t              length = MINIMUM_HEADER_SIZE;
	QuramQmageDecoderHeader  QmageHeader;
	QMINT32				offset = 0;
	QMUCHAR				*pInput = 0;
	unsigned char		cErr = 0;

	SkAutoMalloc		storage;
	SkBitmap::Config    config = SkBitmap::kARGB_8888_Config;
	SkBitmap::Config    prefconfig;
	bool				doDither = this->getDitherImage();
	bool				reuseBitmap = false;


	if (SkImageDecoder::kDecodeBounds_Mode == mode)
	{
		bool streamDataReadDone = false;
#ifdef PRINT_LOG_FOR_DEBUG_FUNC
		ALOGE("onDecode : kDecodeBounds_Mode for parsing");
#endif
		
		// It's not nessecary for parsing to read more than 200 bytes.
		if (pInput == NULL)
		{
			{
				if (stream->hasLength())
				{
					length = stream->getLength();
					if (length < MINIMUM_HEADER_SIZE)
					{
#ifdef PRINT_LOG_FOR_DEBUG
						ALOGV("onDecode : kDecodeBounds_Mode : Header read from file fail");
#endif
						return false;
					}
					pInput = (QMUCHAR*)storage.reset(length);
					if (stream->read(pInput, MINIMUM_HEADER_SIZE) != MINIMUM_HEADER_SIZE)
					{
#ifdef PRINT_LOG_FOR_DEBUG
						ALOGV("onDecode : kDecodeBounds_Mode : Header read from file fail");
#endif
						return false;
					}
				}
				else
				{
					SkDynamicMemoryWStream tempStream;
					// Arbitrary buffer size.
					const size_t bufferSize = MINIMUM_HEADER_SIZE; // 256KB
					char buffer[MINIMUM_HEADER_SIZE];
					do {
						size_t bytesRead = stream->read(buffer, bufferSize);
						tempStream.write(buffer, bytesRead);
						SkDEBUGCODE(debugLength += bytesRead);
						SkASSERT(tempStream.bytesWritten() == debugLength);
					} while (!stream->isAtEnd());
					const size_t length = tempStream.bytesWritten();
					pInput = (QMUCHAR*)storage.reset(length);
					tempStream.copyTo(pInput);
					// Indicate that data has been read completely from stream, no further reads needed.
					streamDataReadDone = true;
				}
			}
		}

		// Parse Header
		if (!QuramQmageDecParseHeader(pInput, QM_IO_BUFFER, MINIMUM_HEADER_SIZE, &QmageHeader))
		{
			//ALOGE("onDecode : kDecodeBounds_Mode : Unknown Header Error!!");
			storage.free();
			return false;
		}
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("Qmage Header Ok");
#endif

		prefconfig = this->getPrefConfig(k32Bit_SrcDepth, true);

		if (QmageHeader.isGrayColor && QmageHeader.IsOpaque)
		{
			if (config != SkBitmap::kRGB_565_Config)
			{
				config = SkBitmap::kARGB_8888_Config;
			}
		}
		else
		{
			prefconfig = SkBitmap::kARGB_8888_Config;
		}
		config = prefconfig;

		int width = QmageHeader.width;
		int height = QmageHeader.height;


		if (QmageHeader.NinePatched) // ninepatch exists, otherwise two values should be same.
		{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
			ALOGE("onDecode : kDecodeBounds_Mode : 9patched image");
#endif
			SkImageDecoder::Peeker* peeker = NULL;
			QuramQmageNinePatchedChunk qchunk;
			QMINT32 patchSize;
		
			if (!streamDataReadDone && stream->read(pInput + MINIMUM_HEADER_SIZE, 12) != 12)
			{
				//ALOGE("onDecode : kDecodeBounds_Mode : NinePatched 1");
				return false;
			}

			if (QM_BOOL_FALSE == QuramQmageDecGetNinePatchedInfo(pInput, length, &qchunk))
			{
				//ALOGE("onDecode : kDecodeBounds_Mode : NinePatched 2 : size 0x%08X  name %s", qchunk.size, qchunk.name);
				return false;
			}
			//if( length < MINIMUM_HEADER_SIZE+12+qchunk.size )
			if (!streamDataReadDone && stream->read(pInput + MINIMUM_HEADER_SIZE + 12, qchunk.size) != qchunk.size)
			{
				//ALOGE("onDecode : kDecodeBounds_Mode : NinePatched 3 : 0x%08X  name %s", qchunk.size, qchunk.name);
				return false;
			}

			if (this->getPeeker())
			{

				if (!strcmp(qchunk.name, "npTL"))
				{
					QuramQmageNinePatchedChunk chunk_TC;
					QuramQmageNinePatchedChunk chunk_LB;
					QMUCHAR* qcdata = qchunk.data_ptr;


					int LB_ptr = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					//strcpy( chunk_TC.name, qcdata + 12, 4 );
					chunk_TC.name[0] = qcdata[4];
					chunk_TC.name[1] = qcdata[5];
					chunk_TC.name[2] = qcdata[6];
					chunk_TC.name[3] = qcdata[7];
					chunk_TC.name[4] = '\0';
					chunk_TC.size = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					chunk_TC.data_ptr = qcdata + 8;

#ifdef PRINT_LOG_FOR_DEBUG
					ALOGE("onDecode : QuramQmageNinePatchedChunk call : %c%c%c%c", chunk_TC.name[0], chunk_TC.name[1], chunk_TC.name[2], chunk_TC.name[3]);
					ALOGE("Qmage QmageDecParseHeader chunk_TC.size %d ", chunk_TC.size);
#endif
					sk_read_np_chunk((void*)this->getPeeker(), &chunk_TC);

					qcdata += LB_ptr + 8;

					chunk_LB.name[0] = qcdata[4];
					chunk_LB.name[1] = qcdata[5];
					chunk_LB.name[2] = qcdata[6];
					chunk_LB.name[3] = qcdata[7];
					chunk_LB.name[4] = '\0';
					chunk_LB.size = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					chunk_LB.data_ptr = qcdata + 8;
					//ALOGE("Qmage QmageDecParseHeader chunk_LB.size %d ", chunk_LB.size);
					sk_read_np_chunk((void*)this->getPeeker(), &chunk_LB);
				}
				else
				{
					sk_read_np_chunk((void*)this->getPeeker(), &qchunk);
				}
			}
		}

		if (!this->chooseFromOneChoice(config, QmageHeader.width, QmageHeader.height))
		{
			//ALOGE("onDecode : kDecodeBounds_Mode : this->chooseFromOneChoice fail");
			return false;
		}

		const int sampleSize = this->getSampleSize();
		SkScaledBitmapSampler sampler(QmageHeader.width, QmageHeader.height, sampleSize);

		// we must always return the same config, independent of mode, so if we were
		// going to respect prefConfig, it must have happened by now

		bm->lockPixels();
		void* rowptr = (void*)bm->getPixels();
		reuseBitmap = (rowptr != NULL);
		bm->unlockPixels();


		if (reuseBitmap && (sampler.scaledWidth() != bm->width() || sampler.scaledHeight() != bm->height()))
		{
			// Dimensions must match
			//ALOGE("onDecode : kDecodeBounds_Mode : width/height not match");
			return false;
		}



		if (!reuseBitmap)
		{
			bm->setConfig(config, sampler.scaledWidth(),
				sampler.scaledHeight(), 0);
		}
	}	// end of if (SkImageDecoder::kDecodeBounds_Mode == mode)
	else
	{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("This is decoding");
#endif
		if (stream->hasLength())
		{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
			ALOGE("decoding stream->hasLength()");
#endif
			pInput = (QMUCHAR *)stream->getMemoryBase();
			length = stream->getLength();
		}
		else
		{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
			ALOGE("decoding stream->hasLength() ELSE");
#endif
			pInput == NULL;
		}

		if (pInput == NULL)
		{
			//pInput	= (QMUCHAR *)storage.reset(allocSize);		// 130522 Cheus

			//nReadLength = stream->read(pInput, length);
			length = CopyStreamToStorage(&storage, stream);
			if (length <= 0)
			{
				storage.free();
				ALOGE("onDecode : stream->read failed read bytes : %d bytes", length);
				return false;
			}
			pInput = (QMUCHAR*)storage.get();
		}

		// Parse Header
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("onDecode : QmageDecParseHeader call : %c%c", pInput[0], pInput[1]);
#endif
		if (!QuramQmageDecParseHeader(pInput, QM_IO_BUFFER, length, &QmageHeader))
		{
			//ALOGE("onDecode : QmageDecParseHeader Error : %c%c", pInput[0], pInput[1]);
			return false;
		}

#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("Qmage parsing for decoding ok");
#endif
		// 100714 Cheus
		SkBitmap::Config    config = SkBitmap::kARGB_8888_Config;
		SkBitmap::Config    prefconfig;
		bool  doDither = this->getDitherImage();			// 100719 Cheus : test

		prefconfig = this->getPrefConfig(k32Bit_SrcDepth, true);

		// now match the request against our capabilities
		if (QmageHeader.IsOpaque)
		{
			if (config != SkBitmap::kRGB_565_Config)
			{
				config = SkBitmap::kARGB_8888_Config;
			}
		}
		else
		{
			prefconfig = SkBitmap::kARGB_8888_Config;
		}

		config = prefconfig;

		int width = QmageHeader.width;
		int height = QmageHeader.height;

#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("onDecode :  QmageHeader.NinePatched %d", QmageHeader.NinePatched);
#endif

		if (QmageHeader.NinePatched) // ninepatch exists, otherwise two values should be same.
		{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
			ALOGE("onDecode : QmageDecParseHeader 9patched image");
#endif
			SkImageDecoder::Peeker* peeker = NULL;
			QuramQmageNinePatchedChunk qchunk;
			QMINT32 patchSize;

			if (QM_BOOL_FALSE == QuramQmageDecGetNinePatchedInfo(pInput, length, &qchunk))
			{
#ifdef PRINT_LOG_FOR_DEBUG
				ALOGE("onDecode : QuramQmageDecGetNinePatchedInfo Fail");
#endif
				return false;
			}

			if (this->getPeeker())
			{
				if (!strcmp(qchunk.name, "npTL"))
				{
					QuramQmageNinePatchedChunk chunk_TC;
					QuramQmageNinePatchedChunk chunk_LB;
					QMUCHAR* qcdata = qchunk.data_ptr;

					int LB_ptr = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					chunk_TC.name[0] = qcdata[4];
					chunk_TC.name[1] = qcdata[5];
					chunk_TC.name[2] = qcdata[6];
					chunk_TC.name[3] = qcdata[7];
					chunk_TC.name[4] = '\0';
					chunk_TC.size = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					chunk_TC.data_ptr = qcdata + 8;
#ifdef PRINT_LOG_FOR_DEBUG
					ALOGE("onDecode : QuramQmageNinePatchedChunk call : %c%c%c%c", chunk_TC.name[0], chunk_TC.name[1], chunk_TC.name[2], chunk_TC.name[3]);
					ALOGE("Qmage QmageDecParseHeader chunk_TC.size %d ", chunk_TC.size);
#endif
					sk_read_np_chunk((void*)this->getPeeker(), &chunk_TC);

					qcdata += LB_ptr + 8;

					chunk_LB.name[0] = qcdata[4];
					chunk_LB.name[1] = qcdata[5];
					chunk_LB.name[2] = qcdata[6];
					chunk_LB.name[3] = qcdata[7];
					chunk_LB.name[4] = '\0';
					chunk_LB.size = (qcdata[0] << 24) | (qcdata[1] << 16) | (qcdata[2] << 8) | (qcdata[3]);
					chunk_LB.data_ptr = qcdata + 8;
					//ALOGE("Qmage QmageDecParseHeader chunk_LB.size %d ", chunk_LB.size);
					sk_read_np_chunk((void*)this->getPeeker(), &chunk_LB);
				}
				else
				{
#ifdef PRINT_LOG_FOR_DEBUG
					ALOGE("onDecode : QuramQmageNinePatchedChunk call44 : %c%c%c%c", qchunk.name[0], qchunk.name[1], qchunk.name[2], qchunk.name[3]);
					ALOGE("Qmage QmageDecParseHeader qchunk.size %d ", qchunk.size);
#endif
					sk_read_np_chunk((void*)this->getPeeker(), &qchunk);
				}
			}
		}

		if (!this->chooseFromOneChoice(config, QmageHeader.width, QmageHeader.height))
		{
			//ALOGE("onDecode : NinePatched : chooseFromOneChoice fail : %c%c", pInput[0], pInput[1]);
			return false;
		}

		const int sampleSize = this->getSampleSize();
		SkScaledBitmapSampler sampler(QmageHeader.width, QmageHeader.height, sampleSize);
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
		ALOGE("onDecode : QmageHeader Height() %d Width() : %d sampleSize : %d", QmageHeader.height, QmageHeader.width, sampleSize);
#endif
		// we must always return the same config, independent of mode, so if we were
		// going to respect prefConfig, it must have happened by now

		bm->setConfig(config, sampler.scaledWidth(), sampler.scaledHeight(), 0);

		//MAKE INDEX COLOR TABLE
		//handling the index8 type
		SkPMColor colorTable[256];
		SkPMColor* colorPtr = colorTable;
		SkColorTable *colorTablep;
		int           colorCount = 0;
		bool reallyHasAlpha = false;
		
		if (QmageHeader.UseIndexedColor)
		{
#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
			ALOGE("Qmage Make Color table\n");
#endif
			colorCount = QmageHeader.ColorCount;

			if (colorCount == 0)
			{
				return false;
			}

			if (QmageHeader.IsOpaque) {
				bm->setAlphaType(kUnpremul_SkAlphaType);
			}
				
			//MAKE A COLOR TABLE
			if (QuramQmageMakeColorTable(pInput, length, colorPtr) == QM_BOOL_FALSE)
			{
				return false;
			}
		}

		if (!reuseBitmap)
		{
			if (!this->allocPixelRef(bm, NULL))
			{
				return false;
			}
		}

		SkAutoLockPixels alp(*bm);
		// case others:        
		// alloc the decode buffer
#ifdef PRINT_LOG_FOR_DEBUG
		//ALOGE("SkAutoMalloc decBuf");
#endif

		if (!QmageHeader.IsOpaque) {
			reallyHasAlpha = true;
		}
		//DECODEING
		if (QmageHeader.UseIndexedColor)
		{
			unsigned char *decPtr;
			int i = 0;

			if (1 == sampleSize)
			{

#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
				ALOGE("SkBitmap::kIndex8_Config == config && 1 == sampleSize");
#endif
				unsigned char* pDecBuf = (unsigned char *)malloc(sizeof(unsigned char)* width * height + 1024);
				offset = QuramQmageDecodeFrame(pInput, length, (QMUCHAR *)pDecBuf);
				reallyHasAlpha = true;
				
				if (offset <= 0)
				{
					ALOGE("Qmage decode fail");
					return false;
				}

				bm->lockPixels();
				colorPtr = colorTable;

				unsigned int * ptr = (unsigned int *)bm->getPixels();
				decPtr = (unsigned char *)pDecBuf;

				for (i = 0; i < width * height; i++)
				{
					*ptr++ = colorPtr[*decPtr++];
				}

				bm->unlockPixels();
				free(pDecBuf);
			}
			else
			{
				SkAutoMalloc        Outstorage;
				QMUCHAR *pDecBuf = 0;
				QMUCHAR *pDecQmageBuf = 0;
				SkScaledBitmapSampler::SrcConfig sc;
				int srcBytesPerPixel = 4;

				if (QmageHeader.transparency) {
					sc = SkScaledBitmapSampler::kRGBA;
				}
				else {
					sc = SkScaledBitmapSampler::kRGBX;
				}

				if (!sampler.begin(bm, sc, *this, NULL))
				{
					return false;
				}

				long AllocSize = length;

				pDecQmageBuf = (QMUCHAR *)Outstorage.reset(width*height * 4 + sizeof(int));
				pDecBuf = (unsigned char *)malloc(sizeof(unsigned int)* width * height + 1024);
				
				unsigned int * ptr = (unsigned int *)pDecQmageBuf;
				offset = QuramQmageDecodeFrame(pInput, length, pDecBuf);
				if (offset <= 0)
				{
					ALOGE("Qmage Decode Error!!!");
					return false;
				}

				decPtr = (unsigned char *)pDecBuf;
				colorPtr = colorTable;
#if 1
				for (i = 0; i < width * height; i++)
				{
					*ptr++ = colorPtr[*decPtr++];
				}
#endif
				// write 
				unsigned char* sampler_base = pDecQmageBuf + sampler.srcY0() * width * srcBytesPerPixel;
				for (int y = 0; y < bm->height(); y++) {
					reallyHasAlpha |= sampler.next(sampler_base);
					sampler_base += sampler.srcDY() * width * srcBytesPerPixel;
				}
				free(pDecBuf);
			}

			SkAlphaType alphaType = kOpaque_SkAlphaType;
			if (reallyHasAlpha) {
				if (QmageHeader.IsOpaque) {
					alphaType = kUnpremul_SkAlphaType;
				}
				else {
					alphaType = kPremul_SkAlphaType;
				}
			}

			colorTablep = SkNEW_ARGS(SkColorTable,
				(colorTable, colorCount, alphaType));

			SkAutoUnref aur(colorTablep);
		}
		else
		{
#ifdef PRINT_LOG_FOR_DEBUG
			ALOGE("normal image decoding\n");
#endif
			if (1 == sampleSize && config != SkBitmap::kRGB_565_Config)
			{
				//ALOGE("onDecode : QmageDecodeFrame call : %c%c", pInput[0], pInput[1]);
				offset = QuramQmageDecodeFrame(pInput, length, (QMUCHAR *)bm->getAddr8(0, 0));

				if (offset <= 0)
				{
					storage.free();
					//ALOGE("onDecode : QmageDecodeFrame fail : %c%c", pInput[0], pInput[1]);
					return false;
				}
			}
			else
			{
				SkAutoMalloc        Outstorage;
				QMUCHAR *pDecBuf = 0;
				SkScaledBitmapSampler::SrcConfig sc;

				int srcBytesPerPixel = 4;

				if (QmageHeader.transparency) {
					sc = SkScaledBitmapSampler::kRGBA;
				}
				else {
					sc = SkScaledBitmapSampler::kRGBX;
				}

				if (!sampler.begin(bm, sc, *this, NULL))
				{
					return false;
				}

				pDecBuf = (QMUCHAR *)Outstorage.reset(width*height * 4 + 1024);				// 130522 Cheus

				offset = QuramQmageDecodeFrame(pInput, length, pDecBuf);

				//index to RGBA
				if (offset <= 0)
				{
					Outstorage.free();
					return false;
				}

				unsigned char* sampler_base = pDecBuf + sampler.srcY0() * width * srcBytesPerPixel;
				for (int y = 0; y < bm->height(); y++) {
					reallyHasAlpha |= sampler.next(sampler_base);
					sampler_base += sampler.srcDY() * width * srcBytesPerPixel;
				}
			}
		}

		SkAlphaType alphaType = kOpaque_SkAlphaType;
		if (reallyHasAlpha) {
			if (QmageHeader.IsOpaque) {
				alphaType = kUnpremul_SkAlphaType;
			}
			else {
				alphaType = kPremul_SkAlphaType;
			}
		}

		if (!reallyHasAlpha)	
			bm->setAlphaType(alphaType);

		if (reuseBitmap){
			bm->notifyPixelsChanged();
		}
	}

#ifdef PRINT_LOG_FOR_DEBUG_FUNC 
	ALOGE("onDecode : return true %c%c", pInput[0], pInput[1]);
#endif
	return true;
}

// amouse
bool SkQmageImageDecoder::onBuildTileIndex(SkStreamRewindable* stream, int *width, int *height)
{
	void* qmage_region_info_ptr = 0;
	SkAutoMalloc		storage;
	unsigned int length;
	unsigned char* pInput;

	if (fImageIndex)
	{
		SkDELETE(fImageIndex);
	}

	length = CopyStreamToStorage(&storage, stream);
	if (length <= 0)
	{
		storage.free();
		ALOGE("onBuildTileIndex : stream->read failed read bytes : %d bytes", length);
		return false;
	}
	pInput = (QMUCHAR*)storage.get();
#ifdef PRINT_LOG_FOR_DEBUG_FUNC
	ALOGE("QuramQmageRegionInit start");
#endif
	// amouse set buffer ptr;
	qmage_region_info_ptr = QuramQmageRegionInit(pInput, length, width, height);
	fImageIndex = SkNEW_ARGS(SkQmageImageIndex, (qmage_region_info_ptr, *width, *height));
#ifdef PRINT_LOG_FOR_DEBUG_FUNC
	ALOGE("QuramQmageRegionInit end");
#endif
	return true;
}

bool SkQmageImageDecoder::onDecodeSubset(SkBitmap* bm, const SkIRect& region)
{
	if (NULL == fImageIndex) {
		return false;
	}

	unsigned int origWidth = fImageIndex->mWidth;
	unsigned int origHeight = fImageIndex->mHeight;


	SkIRect rect = SkIRect::MakeWH(origWidth, origHeight);

	if (!rect.intersect(region)) {
		// If the requested region is entirely outside the image, just
		// returns false
		return false;
	}

	SkBitmap::Config    config = SkBitmap::kARGB_8888_Config;

	const int sampleSize = this->getSampleSize();
	SkScaledBitmapSampler sampler(rect.width(), rect.height(), sampleSize);

	SkBitmap decodedBitmap;
	decodedBitmap.setConfig(config, sampler.scaledWidth(), sampler.scaledHeight());


	// Check ahead of time if the swap(dest, src) is possible.
	// If yes, then we will stick to AllocPixelRef since it's cheaper with the swap happening.
	// If no, then we will use alloc to allocate pixels to prevent garbage collection.
	int w = rect.width() / sampleSize;
	int h = rect.height() / sampleSize;
	const bool swapOnly = (rect == region) && (w == decodedBitmap.width()) &&
		(h == decodedBitmap.height()) && bm->isNull();
	if (swapOnly) {
		if (!this->allocPixelRef(&decodedBitmap, NULL)) {
			return false;
		}
	}
	else {
		if (!decodedBitmap.allocPixels(NULL, NULL)) {
			return false;
		}
	}
	SkAutoLockPixels alp(decodedBitmap);

	/* Turn on interlace handling.  REQUIRED if you are not using
	* png_read_image().  To see how to handle interlacing passes,
	* see the png_read_row() method below:
	*/

	/* Optional call to gamma correct and add the background to the palette
	* and update info structure.  REQUIRED if you are expecting libpng to
	* update the palette for you (ie you selected such a transform above).
	*/

	int actualTop = rect.fTop;
	bool reallyHasAlpha = false;

	{
		SkScaledBitmapSampler::SrcConfig sc;
		int srcBytesPerPixel = 4;
		if (QuramQmageRegionDecoderGetTransparency(fImageIndex->mQmageRegionInfo))
		{
			sc = SkScaledBitmapSampler::kRGBA;
		}
		else {
			sc = SkScaledBitmapSampler::kRGBX;
		}

		/*  We have to pass the colortable explicitly, since we may have one
		even if our decodedBitmap doesn't, due to the request that we
		upscale png's palette to a direct model
		*/
		if (!sampler.begin(&decodedBitmap, sc, *this, 0)) {
			return false;
		}
		const int height = decodedBitmap.height();

		{
			SkAutoMalloc storage(rect.width() * rect.height() * 4);
			uint8_t* base = (uint8_t*)storage.get();
			size_t rb = rect.width() * 4;
#ifdef PRINT_LOG_FOR_DEBUG_FUNC
			ALOGE("QuramQmageDecodeRegion Start");
#endif
			QuramQmageDecodeRegion(fImageIndex->mQmageRegionInfo, rect.x(), rect.y(), rect.width(), rect.height(), base);
#ifdef PRINT_LOG_FOR_DEBUG_FUNC
			ALOGE("QuramQmageDecodeRegion end");
#endif
			// now sample it

			base += sampler.srcY0() * rb;
			for (int y = 0; y < height; y++) {
				// ALOGE("y = %d", y);
				reallyHasAlpha |= sampler.next(base);
				base += sampler.srcDY() * rb;
			}
		}
	}

	SkAlphaType alphaType = kOpaque_SkAlphaType;
	if (reallyHasAlpha) {
		if (!QuramQmageRegionDecoderGetTransparency(fImageIndex->mQmageRegionInfo)){
			alphaType = kUnpremul_SkAlphaType;
		}
		else {
			alphaType = kPremul_SkAlphaType;
		}
	}
	if (!reallyHasAlpha)
		decodedBitmap.setAlphaType(alphaType);

	if (swapOnly) {
		bm->swap(decodedBitmap);
		return true;
	}
	//return decodedBitmap;
	return this->cropBitmap(bm, &decodedBitmap, sampleSize, region.x(), region.y(), region.width(), region.height(), rect.fLeft, rect.fTop);
}

///////////////////////////////////////////////////////////////////////////////

#include "SkTRegistry.h"

static SkImageDecoder* DFactory(SkStreamRewindable* stream) {

	char buffer[MINIMUM_HEADER_SIZE];
	size_t              length = 0;

	if (stream->hasLength())
	{
		length = stream->getLength();

		if (length < MINIMUM_HEADER_SIZE)
		{
			return NULL;
		}

		if (stream->read(buffer, MINIMUM_HEADER_SIZE) != MINIMUM_HEADER_SIZE)
			return NULL;
	}
	else
	{
		SkDynamicMemoryWStream tempStream;
		// Arbitrary buffer size.
		const size_t bufferSize = MINIMUM_HEADER_SIZE;
		size_t read_size = 0;
		char buffert[MINIMUM_HEADER_SIZE];
		do {
			size_t bytesRead = stream->read(buffert, bufferSize);
			tempStream.write(buffert, bytesRead);
			//SkDEBUGCODE(debugLength += bytesRead);
			//SkASSERT(tempStream.bytesWritten() == debugLength);
			read_size += bytesRead;
			if (read_size == bufferSize)
				break;
		} while (!stream->isAtEnd());
		const size_t length = tempStream.bytesWritten();
		tempStream.copyTo(buffer);
	}

	if (QuramQmageDecVersionCheck((QMUCHAR*)buffer) == QM_BOOL_FALSE)
		return NULL;

	if (stream->hasLength())
	{
		size_t len = stream->getLength();
		if (len < MINIMUM_HEADER_SIZE)
			return NULL;
	}

	return SkNEW(SkQmageImageDecoder);
}

static SkImageDecoder_DecodeReg gDReg(DFactory);

