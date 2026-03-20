#pragma once
#include "ObjectDescriptors.h"
#include "CommandList.h"

namespace raphael
{
    class IResource;
    class IPipeline;

    // Device interface
    class IDevice
    {
    public:
        virtual const DeviceDesc& getDesc() const = 0;
        virtual ~IDevice() = default;

    private:
    };

    class IResource
    {
    public:
        virtual const ResourceDesc& getDesc() const = 0;
        virtual ~IResource() = default;
    };

    class IPipeline
    {
    public:
        virtual const PipelineDesc& getDesc() const = 0;
        virtual ~IPipeline() = default;
    };
} // namespace raphael
