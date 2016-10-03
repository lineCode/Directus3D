/*
Copyright(c) 2016 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ===============================
#include "Texture.h"
#include "../Core/GUIDGenerator.h"
#include "../IO/Serializer.h"
#include "../IO/Log.h"
#include "../AssetImporting/ImageImporter.h"
#include "../IO/FileSystem.h"
#include "../Core/Helper.h"
//==========================================

//= NAMESPACES =====
using namespace std;
//==================

Texture::Texture()
{
	m_ID = GENERATE_GUID;
	m_name = "N/A";
	m_filePathTexture = "N/A";
	m_filePathMetadata = "N/A";
	m_width = 0;
	m_height = 0;
	m_shaderResourceView = nullptr;
	m_type = Albedo;
	m_grayscale = false;
	m_transparency = false;
	m_alphaIsTransparency = false;
}

Texture::~Texture()
{
	SafeRelease(m_shaderResourceView);
}

void Texture::Serialize() const
{
	Serializer::WriteSTR(m_ID);
	Serializer::WriteSTR(m_name);
	Serializer::WriteSTR(m_filePathTexture);
	Serializer::WriteSTR(m_filePathMetadata);
	Serializer::WriteInt(m_width);
	Serializer::WriteInt(m_height);
	Serializer::WriteInt(int(m_type));
	Serializer::WriteBool(m_grayscale);
	Serializer::WriteBool(m_transparency);
}

void Texture::Deserialize()
{
	m_ID = Serializer::ReadSTR();
	m_name = Serializer::ReadSTR();
	m_filePathTexture = Serializer::ReadSTR();
	m_filePathMetadata = Serializer::ReadSTR();
	m_width = Serializer::ReadInt();
	m_height = Serializer::ReadInt();
	m_type = TextureType(Serializer::ReadInt());
	m_grayscale = Serializer::ReadBool();
	m_transparency = Serializer::ReadBool();
}

void Texture::SaveToFile(string filePath)
{
	m_filePathTexture = filePath;
	m_filePathMetadata = m_filePathTexture + GetName() + TEXTURE_METADATA_EXTENSION;

	Serializer::StartWriting(m_filePathMetadata);
	Serialize();
	Serializer::StopWriting();
}

// Loads a texture from an image file (.jpg, .png and so on)
bool Texture::LoadFromFile(string path)
{
	// load it
	if (!ImageImporter::GetInstance().Load(path))
	{
		LOG_ERROR("Failed to load texture \"" + path + "\".");
		ImageImporter::GetInstance().Clear();
		return false;
	}

	// Get metadata from texture
	SetFilePathTexture(ImageImporter::GetInstance().GetPath());
	SetFilePathMetadata(GetFilePathTexture() + GetName() + TEXTURE_METADATA_EXTENSION);
	SetName(FileSystem::GetFileNameNoExtensionFromPath(GetFilePathTexture()));
	SetWidth(ImageImporter::GetInstance().GetWidth());
	SetHeight(ImageImporter::GetInstance().GetHeight());
	SetGrayscale(ImageImporter::GetInstance().IsGrayscale());
	SetTransparency(ImageImporter::GetInstance().IsTransparent());
	m_shaderResourceView = ImageImporter::GetInstance().GetAsD3D11ShaderResourceView();

	// Clear any memory allocated by the image loader
	ImageImporter::GetInstance().Clear();

	// Load metadata file (if it exists)
	LoadMetadata(GetFilePathMetadata());

	return true;
}

bool Texture::LoadMetadata(const string& filePath)
{
	if (!FileSystem::FileExists(filePath) || !FileSystem::IsSupportedImage(filePath))
		return false;

	Serializer::StartReading(filePath);
	Deserialize();
	Serializer::StopReading();

	return true;
}