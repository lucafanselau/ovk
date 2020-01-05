#pragma once

#include "handle.h"
#include "framebuffer.h"

namespace ovk {

	class Device;
	class Surface;

	struct OVK_API SwapChainSupport {
		vk::SurfaceCapabilitiesKHR capabilities = {};
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> present_modes;
	};

	/**
	 * \brief USING SWAPCHAIN REQUIRES SWAPCHAIN_KHR EXTENSIONS
	 *				Swapchain creates a simple optimal Swapchain for multi-buffered Rendering
	 */
	class OVK_API SwapChain : public DeviceObject<vk::SwapchainKHR> {
	private:
		SwapChain(Surface& surface, Device& device);
		friend class Device;
		static SwapChainSupport query_support(Surface& s, Device& d);

	public:

		vk::AttachmentDescription get_color_attachment_description() const;
		std::vector<ovk::Framebuffer> create_framebuffers(ovk::RenderPass& rp, ovk::Device& device);
		
		vk::SurfaceFormatKHR format{};
		vk::PresentModeKHR present_mode;
		vk::Extent2D swap_extent;
		uint32_t image_count;

		std::vector<vk::Image> images;
		std::vector<UniqueHandle<vk::ImageView>> image_views;

	};

}
