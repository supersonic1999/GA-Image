//
//  objImporter.h
//

#pragma once

#pragma warning( disable : 4275 )
#pragma warning( disable : 4251 )
#pragma warning( disable : 4231 )

#include <CGImport\CGImport3.h>

class CGModel;

CGIMPORT3_API CG_IMPORT_RESULT importOBJ(const wchar_t *filename, CGModel *model, bool _mapToTextureTopology = false);

