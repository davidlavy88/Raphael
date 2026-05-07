#pragma once
#include <memory>
#include "DX12/ResourceDx12.h"
#include "DX12/DescriptorHeapDx12.h"
#include "DX12/ObjectDescriptors.h"
#include "DX12/DeviceDx12.h"
#include "DX12/CommandList.h"

namespace raphael
{
	class Texture
	{
	public:
		Texture() = default;
		~Texture() = default;

		void Initialize(std::shared_ptr<DescriptorHeapDx12>& srvHeap, DeviceDx12* device);

		void LoadTextureFromDDSFile(const std::string& filename, DeviceDx12* device, CommandList* commandList);
		void LoadTextureFromWICFile(const std::string& filename, DeviceDx12* device, CommandList* commandList);
		void CreateDummyTexture(DeviceDx12* device, CommandList* commandList);
		
		ResourceDx12* GetResource() const { return m_defaultResource.get(); }
		ResourceView GetResourceView() const { return m_srvResourceView; }

	private:
		std::unique_ptr<ResourceDx12> m_defaultResource;
		std::unique_ptr<ResourceDx12> m_uploadResource;
		std::shared_ptr<DescriptorHeapDx12> m_srvDescriptorHeap;
		ResourceView m_srvResourceView;

		std::string m_name;
		std::string m_filename;
	};
}
