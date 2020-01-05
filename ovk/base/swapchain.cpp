#include "pch.h"
#include "swapchain.h"


#include "device.h"
#include "surface.h"

namespace ovk {

	vk::SurfaceFormatKHR choose_format(const std::vector<vk::SurfaceFormatKHR>& available) {
		if (available.size() == 1 && available[0].format == vk::Format::eUndefined) {
			vk::SurfaceFormatKHR ret;
			ret.format = vk::Format::eB8G8R8A8Unorm;
			ret.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
			return ret;
		}

		for (const auto& available_format : available) {
			if (available_format.format == vk::Format::eB8G8R8A8Unorm &&
				available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear )
				return available_format;
		}

		return available[0];
	}

	vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& available) {
		auto best_choise = vk::PresentModeKHR::eFifo;

		/*for (const auto& available_mode : available) {
			if (available_mode == vk::PresentModeKHR::eMailbox) {
				return available_mode;
			}
			else if (available_mode == vk::PresentModeKHR::eImmediate) {
				best_choise = available_mode;
			}
		}*/

		return best_choise;
	}

	vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		vk::Extent2D actual = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actual.width = std::clamp(actual.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actual.height = std::clamp(actual.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actual;

	}


	SwapChain::SwapChain(Surface& surface, Device& device) : DeviceObject(device.device.get()) {

		const auto support = query_support(surface, device);

		auto image_count = support.capabilities.minImageCount + 1;
		if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount)
			image_count = support.capabilities.maxImageCount;


		format = choose_format(support.formats);
		present_mode = choose_present_mode(support.present_modes);
		swap_extent = choose_swap_extent(support.capabilities, surface.window.get());

		vk::SwapchainCreateInfoKHR create_info{
			{},
			surface.surface.get(),
			image_count,
			format.format,
			format.colorSpace,
			swap_extent,
			1,
			vk::ImageUsageFlagBits::eColorAttachment,
			{}, {}, {},
			support.capabilities.currentTransform,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			present_mode,
			true,
			{}
		};

		if (device.families.graphics != device.families.present) {
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			uint32_t queue_indices[] = { device.families.graphics.value(), device.families.present.value() };
			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = queue_indices;
		} else {
			create_info.imageSharingMode = vk::SharingMode::eExclusive;
		}

		handle.set(VK_CREATE(device.device->createSwapchainKHR(create_info), "Failed to create Swapchain!"));

		images = VK_GET(device.device->getSwapchainImagesKHR(handle.get()));

		this->image_count = static_cast<uint32_t>(images.size());

		image_views.reserve(images.size());

		for (auto image : images) {
			
			vk::ImageViewCreateInfo image_view_create_info{
				{},
				image,
				vk::ImageViewType::e2D,
				format.format,
				vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			};

			UniqueHandle<vk::ImageView> image_view(
				VK_CREATE(device.device->createImageView(image_view_create_info), "Failed to create Swapchain Image View"),
				ObjectDestroy<vk::ImageView>(device.device.get())
			);

			image_views.push_back(std::move(image_view));

		}

	}

	SwapChainSupport SwapChain::query_support(Surface &surface, Device &device) {

		SwapChainSupport support;

		support.capabilities = VK_GET(device.physical_device.getSurfaceCapabilitiesKHR(surface.surface.get()));

		support.formats = VK_GET(device.physical_device.getSurfaceFormatsKHR(surface.surface.get()));
		support.present_modes = VK_GET(device.physical_device.getSurfacePresentModesKHR(surface.surface.get()));

		return support;
	}

	vk::AttachmentDescription SwapChain::get_color_attachment_description() const {
		return vk::AttachmentDescription{
		{}, format.format, vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eClear, {},
		vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
		{}, vk::ImageLayout::ePresentSrcKHR
		};
	}

	std::vector<ovk::Framebuffer> SwapChain::create_framebuffers(ovk::RenderPass &rp, ovk::Device& device) {
		std::vector<ovk::Framebuffer> framebuffers;
		for (auto& swap_image : image_views) {
			framebuffers.push_back(
				std::forward<ovk::Framebuffer>(
					device.create_framebuffer(
						rp,
						vk::Extent3D(swap_extent.width, swap_extent.height, 1),
						{ swap_image })));
		}

		return std::move(framebuffers);
	}
}
