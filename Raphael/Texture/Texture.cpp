#include "Texture.h"
#include "TextureLoader/DDSTextureLoader.h"
#include "TextureLoader/WICTextureLoader12.h"

namespace raphael
{
	void Texture::Initialize(std::shared_ptr<DescriptorHeapDx12>& srvHeap, /*REMOVE THIS*/ DeviceDx12* device)
	{
		m_srvDescriptorHeap = srvHeap;
	}

	void Texture::LoadTextureFromDDSFile(const std::string& filename, DeviceDx12* device, CommandList* commandList)
	{
		ComPtr<ID3D12Resource> textureResource;
		ComPtr<ID3D12Resource> uploadResource;

		if (FAILED(DirectX::CreateDDSTextureFromFile12(
			device->getNativeDevice(),
			commandList->getNativeCommandList(),
			std::wstring(filename.begin(), filename.end()).c_str(),
			textureResource,
			uploadResource)))
		{
			throw std::runtime_error("Failed to load texture " + std::string("Textures/WoodCrate01.dds"));
		}

		m_defaultResource = std::make_unique<ResourceDx12>(device, textureResource);
		m_uploadResource = std::make_unique<ResourceDx12>(device, uploadResource);

		DescriptorHandle srvHandle = {};
		m_srvDescriptorHeap->AllocateHeap(&srvHandle);
		m_srvResourceView = m_defaultResource->getResourceView(ResourceBindFlags::ShaderResource, srvHandle);
	}

	void Texture::LoadTextureFromWICFile(const std::string& filename, DeviceDx12* device, CommandList* commandList)
	{
		// WIC loader needs these additional outputs
		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource = {};

		ComPtr<ID3D12Resource> textureResource;

		// Load the texture from file
		HRESULT hr = DirectX::LoadWICTextureFromFile(
			device->getNativeDevice(),
			std::wstring(filename.begin(), filename.end()).c_str(),
			textureResource.GetAddressOf(),  // Creates the texture resource
			decodedData,                            // Stores decoded pixel data
			subresource);                           // Contains upload info

		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to load texture " + std::string(filename.begin(), filename.end()));
		}

		// Query the texture resource to calculate how many bytes the staging buffer needs
		const UINT64 textureBufferSize = GetRequiredIntermediateSize(
			textureResource.Get(), 0, 1);

		// Create texture buffer resource
		ResourceDesc textureUploadDesc = {};
		textureUploadDesc.type = ResourceDesc::ResourceType::Buffer;
		textureUploadDesc.usage = ResourceDesc::Usage::Upload;
		textureUploadDesc.width = textureBufferSize;

		m_uploadResource = device->createResource(textureUploadDesc);
		m_defaultResource = std::make_unique<ResourceDx12>(device, textureResource);

		commandList->copyTextureResource(
			m_defaultResource.get(), m_uploadResource.get(), &subresource);

		DescriptorHandle srvHandle = {};
		m_srvDescriptorHeap->AllocateHeap(&srvHandle);
		m_srvResourceView = m_defaultResource->getResourceView(ResourceBindFlags::ShaderResource, srvHandle);
	}
	void Texture::CreateDummyTexture(DeviceDx12* device, CommandList* commandList)
	{
		static const uint32_t whitePixel = 0xFFFFFFFF;
		D3D12_SUBRESOURCE_DATA subresource = {};
		subresource.pData = &whitePixel;
		subresource.RowPitch = sizeof(whitePixel);
		subresource.SlicePitch = sizeof(whitePixel);

		ResourceDesc textureDesc = {};
		textureDesc.type = ResourceDesc::ResourceType::Texture2D;
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.mipLevels = 1;
		textureDesc.format = ResourceFormat::R8G8B8A8_UNORM;
		textureDesc.bindFlags = ResourceBindFlags::ShaderResource;

		auto whiteTextureResource = device->createResource(textureDesc);
		ComPtr<ID3D12Resource> nativeResource = whiteTextureResource->getNativeResource();

		const UINT64 textureBufferSize = GetRequiredIntermediateSize(nativeResource.Get(), 0, 1);

		ResourceDesc textureUploadDesc = {};
		textureUploadDesc.type = ResourceDesc::ResourceType::Buffer;
		textureUploadDesc.usage = ResourceDesc::Usage::Upload;
		textureUploadDesc.width = textureBufferSize;

		m_uploadResource = device->createResource(textureUploadDesc);
		m_defaultResource = std::make_unique<ResourceDx12>(device, nativeResource);

		commandList->copyTextureResource(m_defaultResource.get(), m_uploadResource.get(), &subresource);

		DescriptorHandle srvHandle = {};
		m_srvDescriptorHeap->AllocateHeap(&srvHandle);
		m_srvResourceView = m_defaultResource->getResourceView(ResourceBindFlags::ShaderResource, srvHandle);
	}
}
