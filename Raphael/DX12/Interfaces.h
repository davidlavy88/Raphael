#pragma once
#include "ObjectDescriptors.h"
#include "CommandList.h"

namespace raphael
{
    class IResource;

    // Device interface
    class IDevice
    {
    public:
        virtual const DeviceDesc& getDesc() const = 0;
        virtual std::unique_ptr<IResource> createResource(const ResourceDesc& desc) = 0; // Using double pointer since we need to allocate the resource pointer
        virtual std::unique_ptr<CommandList> createCommandList(const CommandListDesc& desc) = 0; // Using double pointer since we need to allocate the command list pointer
        virtual void executeCommandList(CommandList* commandList) = 0;
        virtual void waitForGpu() = 0;

        virtual ~IDevice() = default;

    private:
    };

    class IResource
    {
    public:
        virtual const ResourceDesc& getDesc() const = 0;
        virtual ~IResource() = default;
    };
} // namespace raphael
