/*
Copyright(c) 2016-2018 Panos Karabelas

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

//= INCLUDES ===========================
#include "RHI_Texture.h"
#include "RHI_Device.h"
#include "../IO/FileStream.h"
#include "../Rendering/Renderer.h"
#include "../Resource/ResourceManager.h"
//======================================

//= NAMESPACES =====
using namespace std;
//==================

namespace Directus
{
	RHI_Texture::RHI_Texture(Context* context) : IResource(context, Resource_Texture)
	{
		m_isUsingMipmaps	= true;
		m_format			= Texture_Format_R8G8B8A8_UNORM;
		m_rhiDevice			= context->GetSubsystem<Renderer>()->GetRHIDevice();
		m_shaderResource	= nullptr;
		m_memoryUsage		= 0;
	}

	//= RESOURCE INTERFACE =====================================================================
	bool RHI_Texture::SaveToFile(const string& filePath)
	{
		return Serialize(filePath);
	}

	bool RHI_Texture::LoadFromFile(const string& rawFilePath)
	{
		m_data.clear();
		m_data.shrink_to_fit();
		SetLoadState(LoadState_Started);

		// Make the path, relative to the engine
		auto filePath = FileSystem::GetRelativeFilePath(rawFilePath);

		// engine format (binary)
		if (FileSystem::IsEngineTextureFile(filePath)) 
		{
			Deserialize(filePath);
		}
		// foreign format (most known image formats)
		else if (FileSystem::IsSupportedImageFile(filePath))
		{
			LoadFromForeignFormat(filePath);
		}
		else
		{
			LOGF_ERROR("RI_Texture::LoadFromFile: Failed to load \"%s\".", filePath.c_str());
			SetLoadState(LoadState_Failed);
			return false;
		}

		bool generateMipmaps = !m_isUsingMipmaps;
		if (ShaderResource_Create2D(m_width, m_height, m_channels, m_format, m_data, generateMipmaps))
		{
			// If the texture was loaded from an image file, it's not 
			// saved yet, hence we have to maintain it's texture bits.
			// However, if the texture was deserialized (engine format) 
			// then we no longer need the texture bits. 
			// We free them here to free up some memory.
			if (FileSystem::IsEngineTextureFile(filePath))
			{
				ClearTextureBytes();
			}
		}
		else
		{
			LOGF_ERROR("RHI_Texture::LoadFromFile: Failed to create shader resource for \"%s\".", m_resourceFilePath.c_str());
		}

		SetLoadState(LoadState_Completed);
		return true;
	}

	unsigned int RHI_Texture::GetMemoryUsage()
	{
		// Compute texture bits (in case they are loaded)
		unsigned int size = 0;
		for (const auto& mip : m_data)
		{
			size += (unsigned int)mip.size();
		}

		return size;
	}
	//=====================================================================================

	Mipmap* RHI_Texture::Data_GetMip(unsigned int index)
	{
		if (index >= m_data.size())
		{
			LOG_WARNING("RHI_Texture::Data_GetMip: Index out of range");
			return nullptr;
		}

		return &m_data[index];
	}

	void RHI_Texture::ClearTextureBytes()
	{
		for (auto& mip : m_data)
		{
			mip.clear();
			mip.shrink_to_fit();
		}
		m_data.clear();
		m_data.shrink_to_fit();
	}

	void RHI_Texture::GetTextureBytes(vector<vector<std::byte>>* textureBytes)
	{
		if (!m_data.empty())
		{
			textureBytes = &m_data;
			return;
		}

		auto file = make_unique<FileStream>(m_resourceFilePath, FileStreamMode_Read);
		if (!file->IsOpen())
			return;

		unsigned int mipCount = file->ReadUInt();
		for (unsigned int i = 0; i < mipCount; i++)
		{
			textureBytes->emplace_back(vector<std::byte>());
			file->Read(&m_data[i]);
		}
	}

	bool RHI_Texture::LoadFromForeignFormat(const string& filePath)
	{
		if (filePath == NOT_ASSIGNED)
		{
			LOG_WARNING("RHI_Texture::LoadFromForeignFormat: Can't load texture, filepath is unassigned.");
			return false;
		}

		// Load texture
		ImageImporter* imageImp = m_context->GetSubsystem<ResourceManager>()->GetImageImporter();	
		if (!imageImp->Load(filePath, (RHI_Texture*)this))
		{
			return false;
		}

		// Change texture extension to an engine texture
		SetResourceFilePath(FileSystem::GetFilePathWithoutExtension(filePath) + EXTENSION_TEXTURE);
		SetResourceName(FileSystem::GetFileNameNoExtensionFromFilePath(GetResourceFilePath()));

		return true;
	}

	bool RHI_Texture::Serialize(const string& filePath)
	{
		// If the texture bits has been cleared, load it again
		// as we don't want to replaced existing data with nothing.
		// If the texture bits are not cleared, no loading will take place.
		GetTextureBytes(&m_data);

		auto file = make_unique<FileStream>(filePath, FileStreamMode_Write);
		if (!file->IsOpen())
			return false;

		// Write texture bits
		file->Write((unsigned int)m_data.size());
		for (auto& mip : m_data)
		{
			file->Write(mip);
		}

		// Write properties
		file->Write(m_bpp);
		file->Write(m_width);
		file->Write(m_height);
		file->Write(m_channels);
		file->Write(m_isGrayscale);
		file->Write(m_isTransparent);
		file->Write(m_isUsingMipmaps);
		file->Write(m_resourceID);
		file->Write(m_resourceName);
		file->Write(m_resourceFilePath);

		ClearTextureBytes();

		return true;
	}

	bool RHI_Texture::Deserialize(const string& filePath)
	{
		auto file = make_unique<FileStream>(filePath, FileStreamMode_Read);
		if (!file->IsOpen())
			return false;

		// Read texture bits
		ClearTextureBytes();
		unsigned int mipCount = file->ReadUInt();
		for (unsigned int i = 0; i < mipCount; i++)
		{
			m_data.emplace_back(vector<std::byte>());
			file->Read(&m_data[i]);
		}

		// Read properties
		file->Read(&m_bpp);
		file->Read(&m_width);
		file->Read(&m_height);
		file->Read(&m_channels);
		file->Read(&m_isGrayscale);
		file->Read(&m_isTransparent);
		file->Read(&m_isUsingMipmaps);
		file->Read(&m_resourceID);
		file->Read(&m_resourceName);
		file->Read(&m_resourceFilePath);

		return true;
	}
}
