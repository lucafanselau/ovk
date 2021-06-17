#include "pch.h"
#include "buffer.h"

#include "device.h"

namespace ovk {

	

	Buffer::~Buffer() {
		// handle.invalidate(true, false, false);
		// memory.invalidate(true, false, false);
	}


	Buffer::Buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, void* data, std::vector<QueueType> types, mem::MemoryType mem_type, mem::Allocator* allocator, Device& device)
		: DeviceObject<vk::Buffer>(device.device.get()),
			size(size), memory_type(mem_type) {
		{
			// Create Buffer Handle
			std::vector<uint32_t> q(types.size());
			for (auto i = 0; i < types.size(); i++) {
				q[i] = device.families.get_family(types[i]);
			}

			if (mem_type == mem::MemoryType::device_local) {
				usage |= vk::BufferUsageFlagBits::eTransferDst;
				if (const auto transfer_family = device.families.get_family(QueueType::transfer); transfer_family != q[0])
					q.push_back(transfer_family);
			}

			vk::BufferCreateInfo create_info{
				{},
				size,
				usage,
				q.size() == 1 ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
				q.size() == 1 ? 0 : static_cast<uint32_t>(q.size()),
				q.size() == 1 ? nullptr : q.data()
			};

			handle.set(VK_CREATE(device.device->createBuffer(create_info), "Failed to create Buffer"));

			// Get Buffer Memory Requirements
			requirements = device.device->getBufferMemoryRequirements(handle.get());

			const mem::AllocateInfo alloc_info{
				mem_type,
				static_cast<uint32_t>(size),
				mem::AllocationFlag::none,
				requirements
			};
			memory = allocator->allocate(alloc_info, device);
			
			device.device->bindBufferMemory(handle.get(), memory->get(), memory->get_offset());
			
		}

		upload(size, data, device);

	}

	void Buffer::upload(vk::DeviceSize size, void *data, Device& device) {

		using mem_prop = vk::MemoryPropertyFlagBits;

#ifdef DEBUG
		if (requirements.size < size) spdlog::warn("[Buffer] (upload) size exceeds buffer requirements size");
#endif

		if (!data) return;
		
		// Fill Buffer
		if (memory_type == mem::MemoryType::device_local) {
			// Staging Upload

			auto& vk_device = device.device.get();

			struct {
				vk::DeviceMemory memory;
				vk::Buffer buffer;
			} staging;

			// TODO: Do automated staging buffer
			staging.buffer = VK_CREATE(device.device->createBuffer({
				{},
				size,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive
				}), "Failed to create staging Buffer");

			const auto staging_requirements = vk_device.getBufferMemoryRequirements(staging.buffer);

			vk::MemoryAllocateInfo staging_alloc_info{
				staging_requirements.size,
				mem::find_memory_type(device.physical_device, mem_prop::eHostCoherent | mem_prop::eHostVisible).value()
			};

			staging.memory = VK_CREATE(vk_device.allocateMemory(staging_alloc_info), "Failed to allocate staging memory");

			VK_ASSERT(vk_device.bindBufferMemory(staging.buffer, staging.memory, 0), "Failed to bind Memory");
			
			const auto staging_data = VK_GET(vk_device.mapMemory(staging.memory, 0, requirements.size, {}));
			memcpy(staging_data, data, size);
			vk_device.unmapMemory(staging.memory);
			
			const auto cmd = device.create_single_submit_cmd(QueueType::transfer);

			vk::BufferCopy copy{
				0, 0, size
			};
			cmd.copyBuffer(staging.buffer, handle.get(), 1, &copy);

			device.flush(cmd, QueueType::transfer, true, true);

			vk_device.destroyBuffer(staging.buffer);
			vk_device.freeMemory(staging.memory);

		}
		else {
			// NOT STAGING
			const auto memory_data = memory->map(device);
			memcpy(memory_data, data, size);
			memory->unmap(device);
			
		}
		
	}
}
