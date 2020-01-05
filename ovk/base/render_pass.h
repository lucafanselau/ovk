#pragma once

#include "handle.h"

namespace ovk {

	struct OVK_API GraphicSubpass {

		explicit GraphicSubpass(std::vector<vk::AttachmentDescription> input = {},
			std::vector<vk::AttachmentDescription> color = {},
			std::vector<vk::AttachmentDescription> resolve = {},
			std::vector<vk::AttachmentDescription> preserve = {},
			vk::AttachmentDescription depth_stencil = {});

		std::vector<vk::AttachmentDescription> input, color, resolve, preserve;
		vk::AttachmentDescription depth_stencil;

	};
	
	class OVK_API RenderPass : public DeviceObject<vk::RenderPass> {
		friend class Device;
		RenderPass(std::vector<vk::AttachmentDescription> attachments, std::vector<GraphicSubpass> subpasses, bool add_external_dependency, Device& d);
	};

}