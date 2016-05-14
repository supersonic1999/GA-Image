
#include "stdafx.h"
#include <wincodec.h>
#include <FreeImage\FreeImagePlus.h>
#include <iostream>
#include <codecvt>
#include "TextureLoader.h"


using namespace std;
using namespace CoreStructures;


// Supported MIPMAP generation mode
static CGMipmapGenMode		mipmapGenMode = CG_NO_MIPMAP_GEN;
static bool					mipmapModeInitialised = false;


static void initialiseMipmapMode() {

	if (glewIsSupported("GL_ARB_framebuffer_object"))
		mipmapGenMode = CG_CORE_MIPMAP_GEN;
	else if (glewIsSupported("GL_EXT_framebuffer_object"))
		mipmapGenMode = CG_EXT_MIPMAP_GEN;
	else
		mipmapGenMode = CG_NO_MIPMAP_GEN;

	mipmapModeInitialised = true;
}


#ifdef __CG_USE_WINDOWS_IMAGING_COMPONENT___

// Windows Imaging Component factory class (singleton)
static IWICImagingFactory	*wicFactory = nullptr;


// Private function to safe release COM interfaces
template <class T>
static inline void SafeRelease(T **comInterface) {

	if (*comInterface) {

		(*comInterface)->Release();
		*comInterface = nullptr;
	}
}


static HRESULT getWICFormatConverter(IWICFormatConverter **formatConverter) {

	if (!formatConverter || !wicFactory)
		return E_FAIL;
	else
		return wicFactory->CreateFormatConverter(formatConverter);
}


// Load and return an IWICBitmap interface representing the image loaded from path.  No format conversion is done here - this is left to the caller so each delegate can apply the loaded image data as needed.
static HRESULT loadWICBitmap(LPCWSTR path, IWICBitmap **bitmap) {

	if (!bitmap || !wicFactory)
		return E_FAIL;

	IWICBitmapDecoder *bitmapDecoder = nullptr;
	IWICBitmapFrameDecode *imageFrame = nullptr;
	IWICFormatConverter *formatConverter = nullptr;

	*bitmap = nullptr;

	// Create image decoder
	HRESULT hr = wicFactory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &bitmapDecoder);

	// Validate number of frames

	UINT numFrames = 0;

	if (SUCCEEDED(hr)) {

		hr = bitmapDecoder->GetFrameCount(&numFrames);
	}

	if (SUCCEEDED(hr) && numFrames>0) {

		// Decode first image frame (default to first frame - for animated types add parameters to select frame later!)
		hr = bitmapDecoder->GetFrame(0, &imageFrame);
	}

	if (SUCCEEDED(hr)) {

		hr = wicFactory->CreateFormatConverter(&formatConverter);
	}

	WICPixelFormatGUID pixelFormat;

	if (SUCCEEDED(hr)) {

		// Check we can convert to the required format GUID_WICPixelFormat32bppPBGRA			
		hr = imageFrame->GetPixelFormat(&pixelFormat);
	}

	BOOL canConvert = FALSE;

	if (SUCCEEDED(hr)) {

		hr = formatConverter->CanConvert(pixelFormat, GUID_WICPixelFormat32bppPBGRA, &canConvert);
	}

	if (SUCCEEDED(hr) && canConvert == TRUE) {

		hr = formatConverter->Initialize(
			imageFrame,						// Input source to convert
			GUID_WICPixelFormat32bppPBGRA,	// Destination pixel format
			WICBitmapDitherTypeNone,		// Specified dither pattern
			NULL,							// Specify a particular palette 
			0.f,							// Alpha threshold
			WICBitmapPaletteTypeCustom		// Palette translation type
			);
	}

	if (SUCCEEDED(hr)) {

		// Convert and create bitmap from converter
		hr = wicFactory->CreateBitmapFromSource(formatConverter, WICBitmapCacheOnDemand, bitmap);
	}


	// Cleanup
	SafeRelease(&formatConverter);
	SafeRelease(&imageFrame);
	SafeRelease(&bitmapDecoder);

	// Return result
	return hr;
}


// Private method to load image using WIC and return new texture object.  If the new texture is created, a non-zero texture object ID (provided by OpenGL) is returned and the new texture is bound to GL_TEXTURE_2D in the currently active texture unit.
static GLuint newTextureFromFile(const wstring& textureFilePath) {

	HRESULT hr = S_OK;

	// On first call instantiate WIC factory class
	if (!wicFactory) {

		hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

		// Validate wicFactory before proceeding
		if (!SUCCEEDED(hr))
			return 0;
	}

	IWICBitmap *textureBitmap = nullptr;
	IWICBitmapLock *lock = nullptr;
	GLuint newTexture = 0;

	hr = loadWICBitmap(textureFilePath.c_str(), &textureBitmap);

	UINT w = 0, h = 0;

	if (SUCCEEDED(hr))
		hr = textureBitmap->GetSize(&w, &h);

	WICRect rect = { 0, 0, w, h };

	if (SUCCEEDED(hr))
		hr = textureBitmap->Lock(&rect, WICBitmapLockRead, &lock);

	UINT bufferSize = 0;
	BYTE *buffer = nullptr;

	if (SUCCEEDED(hr))
		hr = lock->GetDataPointer(&bufferSize, &buffer);

	if (SUCCEEDED(hr)) {

		glGenTextures(1, &newTexture);
		glBindTexture(GL_TEXTURE_2D, newTexture);

		// note: GL_BGRA format used - input image format converted to GUID_WICPixelFormat32bppPBGRA for consistent interface with OpenGL texture setup
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
	}

	SafeRelease(&lock);
	SafeRelease(&textureBitmap);

	return newTexture;
}


//
// Public WIC methods
//

GLuint TextureLoader::wicLoadTexture(const wstring& textureFilePath) {

	GLuint newTexture = newTextureFromFile(textureFilePath);

	// Setup default texture properties
	if (newTexture) {

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	return newTexture;
}


GLuint TextureLoader::wicLoadTexture(const wstring& textureFilePath, const TextureGenProperties& textureProperties) {

	GLuint newTexture = newTextureFromFile(textureFilePath);

	// Setup custom texture properties
	if (newTexture) {

		// Verify we don't use GL_LINEAR_MIPMAP_LINEAR which has no meaning in non-mipmapped textures.  If not set, default to GL_LINEAR (bi-linear) filtering.
		GLint minFilter = (!textureProperties.genMipMaps && textureProperties.minFilter == GL_LINEAR_MIPMAP_LINEAR) ? GL_LINEAR : textureProperties.minFilter;
		GLint maxFilter = (!textureProperties.genMipMaps && textureProperties.maxFilter == GL_LINEAR_MIPMAP_LINEAR) ? GL_LINEAR : textureProperties.maxFilter;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureProperties.anisotropicLevel);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureProperties.wrap_s);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureProperties.wrap_t);

		// Initialise mipmap creation method based on supported extensions
		if (!mipmapModeInitialised)
			initialiseMipmapMode();

		if (textureProperties.genMipMaps && mipmapGenMode != CG_NO_MIPMAP_GEN) {

			if (mipmapGenMode == CG_CORE_MIPMAP_GEN)
				glGenerateMipmap(GL_TEXTURE_2D);
			else if (mipmapGenMode == CG_EXT_MIPMAP_GEN)
				glGenerateMipmapEXT(GL_TEXTURE_2D);
		}
	}

	return newTexture;
}


#endif



//
// FreeImage texture loader public methods
//

GLuint TextureLoader::fiLoadTexture(const wstring& textureFilePath) {

	BOOL				fiOkay = FALSE;
	GLuint				newTexture = 0;
	fipImage			I;

	// Convert wstring to const char*
	wstring_convert<codecvt_utf8<wchar_t>, wchar_t> stringConverter;

	string S = stringConverter.to_bytes(textureFilePath);
	const char *filename = S.c_str();


	// Call FreeImage to load the image file
	fiOkay = I.load(filename);

	if (!fiOkay) {

		cout << "FreeImagePlus: Cannot open image file.\n";
		return 0;
	}

	fiOkay = I.flipVertical();
	fiOkay = I.convertTo24Bits();

	if (!fiOkay) {

		cout << "FreeImagePlus: Conversion to 24 bits successful.\n";
		return 0;
	}

	auto w = I.getWidth();
	auto h = I.getHeight();

	BYTE *buffer = I.accessPixels();

	if (!buffer) {

		cout << "FreeImagePlus: Cannot access bitmap data.\n";
		return 0;
	}


	glGenTextures(1, &newTexture);
	glBindTexture(GL_TEXTURE_2D, newTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGR, GL_UNSIGNED_BYTE, buffer);

	// Setup default texture properties
	if (newTexture) {

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	return newTexture;
}



GLuint TextureLoader::fiLoadTexture(const wstring& textureFilePath, const TextureGenProperties& textureProperties) {

	BOOL				fiOkay = FALSE;
	GLuint				newTexture = 0;
	fipImage			I;

	// Convert wstring to const char*
	wstring_convert<codecvt_utf8<wchar_t>, wchar_t> stringConverter;

	string S = stringConverter.to_bytes(textureFilePath);
	const char *filename = S.c_str();


	// Call FreeImage to load the image file
	fiOkay = I.load(filename);

	if (!fiOkay) {

		cout << "FreeImagePlus: Cannot open image file.\n";
		return 0;
	}

	fiOkay = I.flipVertical();
	fiOkay = I.convertTo24Bits();

	if (!fiOkay) {

		cout << "FreeImagePlus: Conversion to 24 bits successful.\n";
		return 0;
	}

	auto w = I.getWidth();
	auto h = I.getHeight();

	BYTE *buffer = I.accessPixels();

	if (!buffer) {

		cout << "FreeImagePlus: Cannot access bitmap data.\n";
		return 0;
	}


	glGenTextures(1, &newTexture);
	glBindTexture(GL_TEXTURE_2D, newTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGR, GL_UNSIGNED_BYTE, buffer);


	// Setup default texture properties
	if (newTexture) {

		// Verify we don't use GL_LINEAR_MIPMAP_LINEAR which has no meaning in non-mipmapped textures.  If not set, default to GL_LINEAR (bi-linear) filtering.
		GLint minFilter = (!textureProperties.genMipMaps && textureProperties.minFilter == GL_LINEAR_MIPMAP_LINEAR) ? GL_LINEAR : textureProperties.minFilter;
		GLint maxFilter = (!textureProperties.genMipMaps && textureProperties.maxFilter == GL_LINEAR_MIPMAP_LINEAR) ? GL_LINEAR : textureProperties.maxFilter;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, textureProperties.anisotropicLevel);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureProperties.wrap_s);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureProperties.wrap_t);

		// Initialise mipmap creation method based on supported extensions
		if (!mipmapModeInitialised)
			initialiseMipmapMode();
		
		if (textureProperties.genMipMaps && mipmapGenMode != CG_NO_MIPMAP_GEN) {

			if (mipmapGenMode == CG_CORE_MIPMAP_GEN)
				glGenerateMipmap(GL_TEXTURE_2D);
			else if (mipmapGenMode == CG_EXT_MIPMAP_GEN)
				glGenerateMipmapEXT(GL_TEXTURE_2D);
		}
	}

	return newTexture;
}

