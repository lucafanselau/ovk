#include "pch.h"
#include "sync.h"

namespace ovk {

	Fence::Fence(vk::FenceCreateFlags flags, vk::Device &device) : DeviceObject(device) {
		handle.set(VK_CREATE(device.createFence({ flags }), "Failed to create fence"));
	}

	Semaphore::Semaphore(vk::Device &device) : DeviceObject(device, VK_CREATE(device.createSemaphore({}), "Failed to create Semaphore")) {}
	
}
