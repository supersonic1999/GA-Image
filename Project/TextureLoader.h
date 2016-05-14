
// TextureLoader.h - Setup OpenGL textures from images loaded using Windows Imaging Component (WIC) or the FreeImage library


#pragma once

#include <glew\glew.h>
#include <string>


// Structure to define properties for new textures
struct TextureGenProperties {

	GLint		minFilter;
	GLint		maxFilter;
	GLfloat		anisotropicLevel;
	GLint		wrap_s;
	GLint		wrap_t;
	bool		genMipMaps;

	TextureGenProperties(GLint _minf, GLint _maxf, GLfloat _af, GLint _ws, GLint _wt, bool _genmm)
		:minFilter(_minf),
		maxFilter(_maxf),
		anisotropicLevel(_af),
		wrap_s(_ws),
		wrap_t(_wt),
		genMipMaps(_genmm) {}

};


enum CGMipmapGenMode { CG_NO_MIPMAP_GEN = 0, CG_CORE_MIPMAP_GEN, CG_EXT_MIPMAP_GEN };


// Note: Comment this out to remove WIC features from texture_loader
#define __CG_USE_WINDOWS_IMAGING_COMPONENT___		1


class TextureLoader {

public:

#ifdef __CG_USE_WINDOWS_IMAGING_COMPONENT___

	// Windows Imaging Component (WIC) based image loader
	static GLuint wicLoadTexture(const std::wstring& textureFilePath);
	static GLuint wicLoadTexture(const std::wstring& textureFilePath, const TextureGenProperties& textureProperties);

#endif


	// FreeImage-based image loader
	static GLuint fiLoadTexture(const std::wstring& textureFilePath);
	static GLuint fiLoadTexture(const std::wstring& textureFilePath, const TextureGenProperties& textureProperties);
};
