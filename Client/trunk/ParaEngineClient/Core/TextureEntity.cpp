//-----------------------------------------------------------------------------
// Class:	TextureEntity
// Authors:	LiXizhi
// Emails:	LiXizhi@yeah.net
// Company: ParaEngine
// Date:	2004.3.8
// Revised: 2006.7.12, 2009.8.18(AsyncLoader added), 2014.8(cross platform)
//-----------------------------------------------------------------------------
#include "ParaEngine.h"
#include "ContentLoaders.h"
#include "AsyncLoader.h"
#include "ParaWorldAsset.h"
#include "ImageEntity.h"
#include "TextureEntity.h"

using namespace ParaEngine;


namespace ParaEngine
{
	int TextureEntity::g_nTextureLOD = 0;
	bool TextureEntity::g_bEnable32bitsTexture = true;
	const std::string TextureEntity::DEFAULT_STATIC_TEXTURE = "Texture/whitedot.png";

	const TextureEntity::TextureInfo TextureEntity::TextureInfo::Empty(-1, -1, TextureEntity::TextureInfo::FMT_UNKNOWN, TextureEntity::TextureInfo::TYPE_UNKNOWN);

	namespace MipMapping
	{
		inline void average(byte* out, byte* a, byte* b, byte* c, byte* d, int nStride = 3)
		{
			for (int i = 0; i < nStride; ++i)
			{
				out[i] = (byte)(((int)(a[i]) + (int)(b[i]) + (int)(c[i]) + (int)(d[i])) / 4);
			}
		}

		void ScaleMinifyByTwo(byte *Target, byte *Source, int SrcWidth, int SrcHeight, int nStride = 3)
		{
			int x, y, x2, y2;
			int TgtWidth, TgtHeight;
			TgtWidth = std::max(1, SrcWidth / 2);
			TgtHeight = std::max(1, SrcHeight / 2);

			if (SrcWidth == 1)
			{
				for (y = 0; y < TgtHeight; y++) {
					y2 = 2 * y;
					for (x = 0; x < TgtWidth; x++) {
						average(Target + (y * TgtWidth + x)*nStride, Source + (y2 * SrcWidth + x)*nStride, Source + (y2 * SrcWidth + x)*nStride, Source + ((y2 + 1)*SrcWidth + x)*nStride, Source + ((y2 + 1)*SrcWidth + x)*nStride, nStride);
					}
				}
			}
			else if (SrcHeight == 1)
			{
				for (y = 0; y < TgtHeight; y++) {
					for (x = 0; x < TgtWidth; x++) {
						x2 = 2 * x;
						average(Target + (y * TgtWidth + x)*nStride, Source + (y * SrcWidth + x2)*nStride, Source + (y * SrcWidth + x2 + 1)*nStride, Source + (y*SrcWidth + x2)*nStride, Source + (y*SrcWidth + x2 + 1)*nStride, nStride);
					}
				}
			}
			else
			{
				for (y = 0; y < TgtHeight; y++) {
					y2 = 2 * y;
					for (x = 0; x < TgtWidth; x++) {
						x2 = 2 * x;
						average(Target + (y * TgtWidth + x)*nStride, Source + (y2 * SrcWidth + x2)*nStride, Source + (y2 * SrcWidth + x2 + 1)*nStride, Source + ((y2 + 1)*SrcWidth + x2)*nStride, Source + ((y2 + 1)*SrcWidth + x2 + 1)*nStride, nStride);
					}
				}
			}
		};

		int CalculateMipMapPixelCount(int SrcWidth, int SrcHeight, int*pOutLevel = NULL, int nMaxLevel = 10)
		{
			int nCount = SrcWidth * SrcHeight;
			int i = 1;
			for (; i < nMaxLevel; )
			{
				++i;
				SrcWidth = std::max(1, SrcWidth / 2);
				SrcHeight = std::max(1, SrcHeight / 2);
				nCount += SrcWidth * SrcHeight;
				if (SrcWidth == 1 && SrcHeight == 1)
					break;
			}
			if (pOutLevel)
				*pOutLevel = i;
			return nCount;
		}

		void GenerateMipMap(byte * out, byte *Source, int SrcWidth, int SrcHeight, int nStride = 3, int nMaxLevel = 10)
		{
			int i = 1;
			for (; i < nMaxLevel; ++i)
			{
				ScaleMinifyByTwo(out, Source, SrcWidth, SrcHeight, nStride);
				SrcWidth = std::max(1, SrcWidth / 2);
				SrcHeight = std::max(1, SrcHeight / 2);
				Source = out;
				out = out + SrcWidth * SrcHeight * nStride;
				if (SrcWidth == 1 && SrcHeight == 1)
					break;
			}
		}

	}
}

TextureEntity::TextureEntity(const AssetKey& key)
	:AssetEntity(key), SurfaceType(StaticTexture), 	
	m_bAsyncLoad(true), m_bEmbeddedTexture(false),
	m_pRawData(NULL), m_nRawDataSize(0),
	m_pTextureInfo(NULL), m_nHitCount(0), m_dwColorKey(0)
{
}

TextureEntity::TextureEntity()
	: SurfaceType(StaticTexture), m_pTextureInfo(NULL), m_nHitCount(0), m_dwColorKey(0), m_bEmbeddedTexture(false),
	m_pRawData(NULL), m_nRawDataSize(0),
	m_bAsyncLoad(true)
{
}

TextureEntity::~TextureEntity()
{
	SAFE_DELETE_ARRAY(m_pRawData);
	SAFE_DELETE(m_pTextureInfo);
}

char* TextureEntity::GetRawData()
{
	return m_pRawData;
}

int TextureEntity::GetRawDataSize()
{
	return m_nRawDataSize;
}

void TextureEntity::SetRawData(char* pData, int nSize)
{
	SAFE_DELETE_ARRAY(m_pRawData);
	m_pRawData = pData;
	m_nRawDataSize = nSize;
	if (pData)
		SetState(AssetEntity::ASSET_STATE_NORMAL);
}

bool TextureEntity::GiveupRawDataOwnership()
{
	m_pRawData = 0;
	m_nRawDataSize = 0;
	return true;
}

bool TextureEntity::LoadFromImage(ImageEntity * image, D3DFORMAT dwTextureFormat /*= D3DFMT_UNKNOWN*/, UINT nMipLevels, void** ppTexture)
{
	if (image)
	{
		if (LoadFromMemory((const char*)(image->getData()), image->getDataLen(), nMipLevels, dwTextureFormat, ppTexture) == S_OK)
			return true;
	}
	return false;
}

bool TextureEntity::IsAsyncLoad() const
{
	return m_bAsyncLoad;
}

void TextureEntity::SetAsyncLoad(bool val)
{
	m_bAsyncLoad = val;
}

bool TextureEntity::IsFlipY()
{
	return false;
}

void TextureEntity::SetColorKey(Color colorKey) 
{
	m_dwColorKey = colorKey;
}

Color TextureEntity::GetColorKey() 
{ 
	return m_dwColorKey;
}

void TextureEntity::MakeInvalid()
{
	UnloadAsset();
	SurfaceType = StaticTexture;
	SetState(ASSET_STATE_NORMAL);
	SetLocalFileName(CGlobals::GetString().c_str());
	SAFE_DELETE(m_pTextureInfo);
}

void TextureEntity::SetSamplerStateBlocky(bool bIsBlocky)
{

}

bool TextureEntity::IsSamplerStateBlocky()
{
	return false;
}

void TextureEntity::SetTextureInfo(const TextureEntity::TextureInfo& tInfo)
{
	if(SurfaceType!=TextureSequence)
	{
		if(m_pTextureInfo==NULL)
		{
			m_pTextureInfo = new TextureInfo(tInfo);
			if(m_pTextureInfo==NULL)
				OUTPUT_LOG("failed creating texture info for %s\n", GetKey().c_str());
		}
		else
		{
			if(m_pTextureInfo->m_width != tInfo.m_width || m_pTextureInfo->m_height != tInfo.m_height)
			{
				(*m_pTextureInfo) = tInfo;
				if(m_bIsInitialized)
				{
					// reset the render target to the new size
					InvalidateDeviceObjects();
					RestoreDeviceObjects();
				}
			}
		}
	}
}


bool TextureEntity::IsLoaded()
{
	return GetTexture() != 0;
}

bool TextureEntity::IsPending()
{
	return IsLocked() || 
		(GetTexture() == 0 && (GetState() != AssetEntity::ASSET_STATE_FAILED_TO_LOAD) && !(GetKey().empty()));
}

void ParaEngine::TextureEntity::SetImage(ParaImage* pImage)
{
}

bool ParaEngine::TextureEntity::SetRawDataForImage(const char* pData, int nSize, bool bDeleteData)
{
	return false;
}

bool ParaEngine::TextureEntity::SetRawDataForImage(const unsigned char* pixel, size_t datalen, int width, int height, int bitsPerComponent, bool preMulti, bool bDeleteData)
{
	return false;
}

const ParaImage* ParaEngine::TextureEntity::GetImage() const
{
	return nullptr;
}

void ParaEngine::TextureEntity::SwapImage(TextureEntity* other)
{
}

void TextureEntity::SetTextureFPS(float FPS)
{
	if(SurfaceType==TextureSequence)
	{
		// animated texture
		AnimatedTextureInfo* pInfo = GetAnimatedTextureInfo();
		if(pInfo!=0)
			pInfo->m_fFPS = FPS;
	}
}

void TextureEntity::EnableTextureAutoAnimation(bool bEnable)
{
	if(SurfaceType==TextureSequence)
	{
		// animated texture
		AnimatedTextureInfo* pInfo = GetAnimatedTextureInfo();
		if(pInfo!=0)
			pInfo->m_bAutoAnimation = bEnable;
	}
}

void TextureEntity::SetCurrentFrameNumber(int nFrame)
{
	if(SurfaceType==TextureSequence)
	{
		// animated texture
		AnimatedTextureInfo* pInfo = GetAnimatedTextureInfo();
		if(pInfo!=0)
			pInfo->m_nCurrentFrameIndex = nFrame;
	}
}

int TextureEntity::GetCurrentFrameNumber()
{
	if(SurfaceType==TextureSequence)
	{
		// animated texture
		AnimatedTextureInfo* pInfo = GetAnimatedTextureInfo();
		if(pInfo!=0)
			return pInfo->m_nCurrentFrameIndex;
	}
	return 0;
}

int TextureEntity::GetFrameCount()
{
	if(SurfaceType==TextureSequence)
	{
		// animated texture
		AnimatedTextureInfo* pInfo = GetAnimatedTextureInfo();
		if(pInfo!=0)
			return pInfo->m_nFrameCount;
	}
	return 0;
}

TextureEntity::AnimatedTextureInfo* TextureEntity::GetAnimatedTextureInfo()
{
	if(SurfaceType==TextureSequence)
	{
		if(m_pAnimatedTextureInfo==NULL)
		{
			m_pAnimatedTextureInfo = new AnimatedTextureInfo();
			const string& sTextureFileName = GetLocalFileName();
			int nSize = (int)sTextureFileName.size();
			int nTotalTextureSequence = -1;
			if(nSize > 8)
			{
				if(sTextureFileName[nSize-8] == 'a')
				{
					nTotalTextureSequence = 0;
					for (int i=0;i<3;++i)
					{
						char s=sTextureFileName[nSize-5-i];
						if(s>='0' && s<='9')
						{
							nTotalTextureSequence += (int)(s-'0')*(int)pow((float)10,i);
						}
						else
						{
							nTotalTextureSequence = -1;
							break;
						}
					}
					m_pAnimatedTextureInfo->m_nFrameCount = nTotalTextureSequence;
					
					size_t nFrom = sTextureFileName.find("_fps");
					if(nFrom!=-1)
					{
						size_t nTo = sTextureFileName.find('_', nFrom+4);
						if(nTo!=-1)	
						{
							string sFPS = sTextureFileName.substr(nFrom+4, nTo-nFrom-4);
							try
							{
								m_pAnimatedTextureInfo->m_fFPS = (float)atof(sFPS.c_str());
							}
							catch (...)
							{
								OUTPUT_LOG("FPS can not be read from texture sequence file: %s\r\n", sTextureFileName.c_str());
							}
						}
					}
				}
			}
		}
	}
	else
		return NULL;
	return m_pAnimatedTextureInfo;
}

const TextureEntity::TextureInfo* TextureEntity::GetTextureInfo()
{
	return m_pTextureInfo;
}

HRESULT TextureEntity::InitDeviceObjects()
{
	return S_OK;
}

void TextureEntity::Refresh(const char* sFilename, bool bLazyLoad)
{
	if(sFilename != NULL && sFilename[0] != '\0')
	{
		// set valid to true again, just in case the texture file is downloaded. 
		SetLocalFileName(sFilename);

		if (GetState() == AssetEntity::ASSET_STATE_REMOTE)
			SetState(AssetEntity::ASSET_STATE_NORMAL);
	}
	UnloadAsset();

	// make valid again, because we will reload it
	if (!IsValid() && GetState() == AssetEntity::ASSET_STATE_FAILED_TO_LOAD)
	{
		SetState(AssetEntity::ASSET_STATE_NORMAL);
		m_bIsValid = true;
	}

	SAFE_DELETE(m_pTextureInfo);
	
	if(!bLazyLoad)
		LoadAsset();
}


HRESULT TextureEntity::CreateTextureFromFile_Async(void* pContext, RenderDevicePtr pDev /*= NULL*/, const char* sFileName /*= NULL*/, void** ppTexture /*= NULL*/, D3DFORMAT dwTextureFormat /*= D3DFMT_UNKNOWN*/, UINT nMipLevels /*= D3DX_DEFAULT*/, Color dwColorKey /*= 0*/)
{
	if (GetRawData())
	{
		LoadFromMemory(GetRawData(), GetRawDataSize(), nMipLevels, dwTextureFormat, ppTexture);
		return S_OK;
	}

	// Load Texture asynchronously
	asset_ptr<TextureEntity> my_asset(this);
	if (pContext == 0)
		pContext = &(CAsyncLoader::GetSingleton());
	CAsyncLoader* pAsyncLoader = (CAsyncLoader*)pContext;
	if (pAsyncLoader)
	{
		CTextureLoader* pLoader = new CTextureLoader(my_asset, sFileName);
		CTextureProcessor* pProcessor = new CTextureProcessor(my_asset);

		pLoader->m_dwTextureFormat = dwTextureFormat;
		pLoader->m_nMipLevels = nMipLevels;
		pLoader->m_ppTexture = ppTexture;
		pLoader->m_dwColorKey = dwColorKey;

		pProcessor->m_dwTextureFormat = dwTextureFormat;
		pProcessor->m_nMipLevels = nMipLevels;
		pProcessor->m_pDevice = pDev;
		pProcessor->m_ppTexture = ppTexture;
		pProcessor->m_dwColorKey = dwColorKey;

		pAsyncLoader->AddWorkItem(pLoader, pProcessor, NULL, (void**)ppTexture);
	}
	return S_OK;
}

HRESULT TextureEntity::RestoreDeviceObjects()
{
	return S_OK;
}

HRESULT TextureEntity::InvalidateDeviceObjects()
{
	return S_OK;
}

HRESULT TextureEntity::DeleteDeviceObjects()
{
	m_bIsInitialized = false;
	return S_OK;
}

int32 TextureEntity::GetWidth()
{
	const TextureInfo* pInfo = GetTextureInfo();
	return pInfo ? pInfo->GetWidth() : 0;
}

int32 TextureEntity::GetHeight()
{
	const TextureInfo* pInfo = GetTextureInfo();
	return pInfo ? pInfo->GetHeight() : 0;
}

bool TextureEntity::SaveToFile(const char* filename, D3DFORMAT dwFormat, int width, int height, UINT MipLevels /*= 1*/, DWORD Filter /*= D3DX_DEFAULT*/, Color ColorKey /*= 0*/)
{
	return false;
}

void TextureEntity::LoadImage(char *sBufMemFile, int sizeBuf, int &width, int &height, byte ** ppBuffer, bool bAlpha)
{
#ifdef USE_DIRECTX_RENDERER
	TextureEntityDirectX::LoadImage(sBufMemFile, sizeBuf, width, height, ppBuffer, bAlpha);
#elif defined(USE_OPENGL_RENDERER)
	TextureEntityOpenGL::LoadImage(sBufMemFile, sizeBuf, width, height, ppBuffer, bAlpha);
#endif
}

TextureEntity* TextureEntity::CreateTexture(const uint8 * pTexels, int width, int height, int rowLength, int bytesPerPixel, uint32 nMipLevels /*= 0*/, D3DPOOL dwCreatePool/*= D3DPOOL_MANAGED*/, DWORD nFormat /*= 0*/)
{
#ifdef USE_DIRECTX_RENDERER
	return TextureEntityDirectX::CreateTexture(pTexels, width, height, rowLength, bytesPerPixel, nMipLevels, dwCreatePool, nFormat);
#elif defined(USE_OPENGL_RENDERER)
	return TextureEntityOpenGL::CreateTexture(pTexels, width, height, rowLength, bytesPerPixel, nMipLevels, dwCreatePool, nFormat);
#else
	return NULL;
#endif
}

TextureEntity* TextureEntity::CreateTexture(const char* pFileName, uint32 nMipLevels /*= 0*/, D3DPOOL dwCreatePool /*= D3DPOOL_MANAGED*/)
{
#ifdef USE_DIRECTX_RENDERER
	return TextureEntityDirectX::CreateTexture(pFileName, nMipLevels, dwCreatePool);
#elif defined(USE_OPENGL_RENDERER)
	return TextureEntityOpenGL::CreateTexture(pFileName, nMipLevels, dwCreatePool);
#else
	return NULL;
#endif
}

bool TextureEntity::LoadImageOfFormatEx(const std::string& sTextureFileName, char *sBufMemFile, int sizeBuf, int &width, int &height, byte ** ppBuffer, int* pBytesPerPixel, int nFormat, ImageExtendInfo *info)
{
#ifdef USE_DIRECTX_RENDERER
	return TextureEntityDirectX::LoadImageOfFormatEx(sTextureFileName, sBufMemFile, sizeBuf, width, height, ppBuffer, pBytesPerPixel, nFormat, info);
#else
	return false;
#endif
}

bool TextureEntity::LoadImageOfFormat(const std::string& sTextureFileName, char *sBufMemFile, int sizeBuf, int &width, int &height, byte ** ppBuffer, int* pBytesPerPixel, int nFormat)
{
#ifdef USE_DIRECTX_RENDERER
	return TextureEntityDirectX::LoadImageOfFormat(sTextureFileName, sBufMemFile, sizeBuf, width, height, ppBuffer, pBytesPerPixel, nFormat);
#elif defined(USE_OPENGL_RENDERER)
	return TextureEntityOpenGL::LoadImageOfFormat(sTextureFileName, sBufMemFile, sizeBuf, width, height, ppBuffer, pBytesPerPixel, nFormat);
#else
	return false;
#endif
}

int TextureEntity::GetFormatByFileName(const std::string& filename)
{
	int dwTextureFormat = -1; //  FREE_IMAGE_FORMAT    -1: FIF_UNKNOWN;
	int nSize = (int)filename.size();
	if (nSize >= 3)
	{
		// to lower case
#define MAKE_LOWER(c)  if((c)>='A' && (c)<='Z'){(c) = (c)-'A'+'a';}
		char c1 = filename[nSize - 3]; MAKE_LOWER(c1)
			char c2 = filename[nSize - 2]; MAKE_LOWER(c2)
			char c3 = filename[nSize - 1]; MAKE_LOWER(c3)

			// if it is png file we will use dxt3 internally since it contains alpha. 
			if (c1 == 'd' && c2 == 'd' && c3 == 's')
				dwTextureFormat = 24; //  FIF_DDS;
			else if (c1 == 'p' && c2 == 'n' && c3 == 'g')
				dwTextureFormat = 13; //  FIF_PNG;
			else if (c1 == 'j' && c2 == 'p' && c3 == 'g')
				dwTextureFormat = 2; //  FIF_JPEG;
			else if (c1 == 't' && c2 == 'g' && c3 == 'a')
				dwTextureFormat = 17; //  FIF_TARGA;
	}
	return dwTextureFormat;
}

void TextureEntity::SetTextureFramePointer(int framePointer)
{
	uint8_t * frames = (uint8_t*)framePointer;
	int width = GetWidth();
	int height = GetHeight();
	if (m_bRABG) { //RGBA转成ARGB
		static int i, j, r, g, b, a,index;
		for (i = 0; i < width; i++) {
			for (j = 0; j < height; j++) {
				index = (j * 4 * width) + 4 * i;
				g = frames[index + 0]; //绿色 g
				//r = frames[index + 1]; //红色 r
				a = frames[index + 2]; //透明度 a 
				//b = frames[index + 3]; //蓝色 b

				frames[index + 0] = a;
				//frames[index + 1] = r;
				frames[index + 2] = g;
				//frames[index + 3] = b;
			}
		}

	}
	this->LoadUint8Buffer(frames, width, height, height, 4);
}

int TextureEntity::InstallFields(CAttributeClass* pClass, bool bOverride)
{
	AssetEntity::InstallFields(pClass, bOverride);

	pClass->AddField("IsRGBA", FieldType_Bool, (void*)SetIsRGBA_s, (void*)0, NULL, NULL, bOverride);
	pClass->AddField("TextureFramePointer", FieldType_Int, (void*)SetTextureFramePointer_s, (void*)0, NULL, NULL, bOverride);
#ifdef USE_DIRECTX_RENDERER
#else
#endif
	return S_OK;
}