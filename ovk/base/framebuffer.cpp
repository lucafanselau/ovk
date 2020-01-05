#include "pch.h"
#include "framebuffer.h"

#include "device.h"

namespace ovk {

	Framebuffer::Framebuffer(RenderPass& render_pass, vk::Extent3D extent, std::vector<vk::ImageView> attachments, Device *device) 
		: DeviceObject(device->device.get()) {
		
		vk::FramebufferCreateInfo create_info{
			{},
			render_pass.handle.get(),
			static_cast<uint32_t>(attachments.size()),
			attachments.data(),
			extent.width,
			extent.height,
			extent.depth
		};

		handle.set(VK_CREATE(device->device->createFramebuffer(create_info), "Failed to create Framebuffer"));
	}
}
