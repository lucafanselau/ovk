#pragma once

#include "handle.h"

#include <set>
#include "mem.h"

namespace ovk {

	enum class QueueType;

	// Ok so we changed the way that we use and allocate memory (see mem.h)
	// So we need to change the Buffer class to work with that type of memory allocation
	// This also means we need to change the way we think about uploading maybe a rewrite
	// would be the best option.

	
	// TODO: Documentation is horrible! :) Buffer that has its own managed Device Memory
	// CPU Accessible or Device Local (Staging)
	class OVK_API Buffer : public DeviceObject<vk::Buffer> {
	public:
		std::shared_ptr<mem::View> memory;
    uint32_t size;
		mem::MemoryType memory_type;

		virtual ~Buffer();

		Buffer(const Buffer &other) = delete;
		Buffer(Buffer &&other) noexcept = default;
		Buffer & operator=(const Buffer &other) = delete;
		Buffer & operator=(Buffer &&other) noexcept = default;
	private:
		friend class Device;
		Buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, void* data, std::vector<QueueType> types, mem::MemoryType mem_type, mem::Allocator* allocator, Device& device);
		
		void upload(vk::DeviceSize size, void* data, Device& device);

		vk::MemoryRequirements requirements;
	};

}
