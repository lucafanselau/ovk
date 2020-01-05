#pragma once

#include "handle.h"

namespace ovk {
	class RenderPass;

	class OVK_API Framebuffer : public DeviceObject<vk::Framebuffer> {
		friend class Device;
		Framebuffer(RenderPass& render_pass, vk::Extent3D extent, std::vector<vk::ImageView> attachments, Device* device);
	};

}