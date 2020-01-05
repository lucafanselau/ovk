#pragma once

#include "handle.h"

namespace ovk {

	class Device;

	struct OVK_API WaitInfo {
		vk::Semaphore semaphore;
		vk::PipelineStageFlags stage;
	};
	
	class OVK_API Fence : public DeviceObject<vk::Fence> {
		friend Device;
		Fence(vk::FenceCreateFlags flags, vk::Device& device);
	};

	class Semaphore : public DeviceObject<vk::Semaphore> {
		friend Device;
		explicit Semaphore(vk::Device& device);
	};

}
