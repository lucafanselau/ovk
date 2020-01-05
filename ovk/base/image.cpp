#include "pch.h"
#include "image.h"

#include "device.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "mem.h"

namespace ovk {

	Image::Image(vk::Image image, vk::DeviceMemory memory, vk::Format format, vk::Extent3D extent, vk::ImageLayout layout, vk::ImageType type, vk::Device device)
		: DeviceObject(device,image),
			format(format), extent(extent), layout(layout), image_type(type) {}



	Image::Image(vk::ImageType image_type, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags flags, vk::ImageTiling tiling, mem::MemoryType mem_type, mem::Allocator* allocator, Device& device)
		: DeviceObject(device.device.get()),
			format(format),
			extent(extent),
			layout(vk::ImageLayout::eUndefined),
			image_type(image_type) {

		const auto layout = vk::ImageLayout::eUndefined;

		vk::ImageCreateInfo create_info{
			{},
			vk::ImageType::e2D,
			format,
			extent,
			1,  // TODO: Mipmapping
			1,
			vk::SampleCountFlagBits::e1,  // TODO: Multisampling
			tiling,
			flags | vk::ImageUsageFlagBits::eTransferDst,
			vk::SharingMode::eExclusive,  // NOTE: MMMMMMM not like in any possible case (eg. Image ressource between async_compute and graphics)
			0,											// TODO: JAJAJA du weißt doch schonnnnn
			nullptr,
			layout
		};
		
		handle.set(VK_CREATE(device.device->createImage(create_info), "Failed to create Brick image"));

		const auto requirements = device.device->getImageMemoryRequirements(handle.get());
		
		const mem::AllocateInfo alloc_info{
			mem_type,
			static_cast<uint32_t>(requirements.size),
			mem::AllocationFlag::non_linear,
			requirements
		};
		memory = allocator->allocate(alloc_info, device);
		
		VK_DASSERT(device.device->bindImageMemory(handle.get(), memory->get(), memory->get_offset()), "Failed to bind image Memory");
		
	}

	Image Image::from_file_2d(const std::string &filename, vk::ImageUsageFlags image_usage, mem::Allocator* allocator, Device &device) {

		// Load Image Data from file
		int tex_width, tex_height, tex_channels;
		stbi_uc* pixels = stbi_load(filename.c_str(), &tex_width, &tex_height, &tex_channels, 0);
		const vk::DeviceSize image_size = tex_width * tex_height * tex_channels;

		ovk_assert(pixels, "[Image] (from_file_2d) failed to load image file: {}", filename);
		
		
		const auto type = vk::ImageType::e2D;
		const vk::Extent3D extent(static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height), 1);
		
		const auto format = [&]() {
			switch (tex_channels) {
			case 1:
				return vk::Format::eR8Unorm;
			case 2:
				return vk::Format::eR8G8Unorm;
			case 3:
				return vk::Format::eR8G8B8Unorm;
			case 4:
				return vk::Format::eR8G8B8A8Unorm;
			default:
				spdlog::error("[Image] (from_file_2d) Unexpected image format! (should actually never happen!)");
				return vk::Format::eUndefined;
			}
		}();

		ovk_assert(format != vk::Format::eUndefined);

		auto image = from_raw_data_2d(format, pixels, tex_channels, extent, image_usage, allocator, device);
		
		stbi_image_free(pixels);
		
		// Ok that should be it return
		return std::move(image);
		
	}

	Image Image::from_raw_data_2d(vk::Format format, uint8_t *pixels, int channels, vk::Extent3D extent, vk::ImageUsageFlags image_usage, mem::Allocator* allocator, Device& d) {

		// Create Suiting vk::Image
		uint32_t image_size = extent.width * extent.height * channels;

		auto staging_buffer = d.create_staging_buffer(pixels, image_size);
		
		//vk::ImageCreateInfo create_info{
		//	{},
		//	vk::ImageType::e2D,
		//	//data,
		//	extent,
		//	1,  // TODO: Mipmapping
		//	1,
		//	vk::SampleCountFlagBits::e1,  // TODO: Multisampling
		//	vk::ImageTiling::eOptimal,
		//	vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
		//	vk::SharingMode::eExclusive,  // NOTE: MMMMMMM not like in any possible case (eg. Image ressource between async_compute and graphics)
		//	0,
		//	nullptr,
		//	layout
		//};

		/*const auto image_handle = VK_CREATE(d.device->createImage(create_info), "Failed to create Brick image");

		const auto requirements = d.device->getImageMemoryRequirements(image_handle);

		vk::MemoryAllocateInfo alloc_info{
			requirements.size,
			ovk::find_memory_type(d.physical_device, requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal).value()
		};

		const auto memory = VK_CREATE(d.device->allocateMemory(alloc_info), "Failed to allocate device memory for image");

		VK_DASSERT(d.device->bindImageMemory(image_handle, memory, 0), "Failed to bind image Memory");*/

		// OK so to  this part we need to abstract away
		//UniqueImage image(image_handle, memory, data, extent, layout, vk::ImageType::e2D, d.device.get());

		// Create Suiting Image
		//
		Image image(
			vk::ImageType::e2D, 
			format, 
			extent, 
			image_usage | vk::ImageUsageFlagBits::eTransferDst,
			vk::ImageTiling::eOptimal,
			mem::MemoryType::device_local,
			allocator,
			d
		);
		
		// Created Image now lets copy over the data from the staging buffer
		
		const auto cmd = d.create_single_submit_cmd(ovk::QueueType::transfer, true);
		
		image.transition_layout(cmd, vk::ImageLayout::eTransferDstOptimal, QueueType::transfer, d);
		// Copy Buffer to Image
		// 
		// Copy Staging Buffer to image
		const vk::BufferImageCopy region{
			0, 0, 0,
			vk::ImageSubresourceLayers {
				vk::ImageAspectFlagBits::eColor,
				0,   // TODO: Mipmapping
				0,
				1
			},
			vk::Offset3D { 0, 0, 0 },
			extent
		};
		cmd.copyBufferToImage(staging_buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
		
		// Then to sampler layout
		image.transition_layout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, QueueType::transfer, d);

		d.flush(cmd, ovk::QueueType::transfer, true, true);
		
		// Ok that should be it return
		return std::move(image);
	}

	void Image::transition_layout(vk::CommandBuffer cmd, vk::ImageLayout new_layout, QueueType queue_type, Device& device) {
		/*const auto cmd = device.create_single_submit_cmd(queue_type, true);*/

		// Are we a depth format???
		vk::ImageAspectFlags aspect_flag = vk::ImageAspectFlagBits::eColor;			
		if (format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD32Sfloat) {
			aspect_flag = vk::ImageAspectFlagBits::eDepth;
		}
		
		vk::ImageMemoryBarrier barrier{
			{}, {},
			layout,
			new_layout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			handle.get(),
			vk::ImageSubresourceRange {
				aspect_flag,
				0, 1,
				0, 1
			}
		};

		vk::PipelineStageFlags src_stage, dst_stage;

		if (layout == vk::ImageLayout::eUndefined && (new_layout == vk::ImageLayout::eTransferDstOptimal || new_layout == vk::ImageLayout::eTransferSrcOptimal)) {
			src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			barrier.srcAccessMask = {};

			dst_stage = vk::PipelineStageFlagBits::eTransfer;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		} else if (layout == vk::ImageLayout::eColorAttachmentOptimal && new_layout == vk::ImageLayout::eTransferSrcOptimal) {
			src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

			dst_stage = vk::PipelineStageFlagBits::eTransfer;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		} else if (layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			src_stage = vk::PipelineStageFlagBits::eTransfer;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;

			dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		} else if (layout == vk::ImageLayout::eDepthStencilAttachmentOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			src_stage = vk::PipelineStageFlagBits::eLateFragmentTests;
			barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			
			dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		} else {
			// PANIC!!!
			panic("unknown layout transition: {} -> {}", vk::to_string(layout), to_string(new_layout));
		}

		cmd.pipelineBarrier(
			src_stage, dst_stage,
			{},
			{},
			{},
			{ barrier }
		);

		/*device.flush(cmd, queue_type, true, false);*/

		layout = new_layout;
		
	}

	void Image::set_layout(vk::ImageLayout l) {
		layout = l;
	}

	ImageView::ImageView(vk::ImageView handle, vk::Device device) : DeviceObject(device, handle) {}

	vk::ComponentSwizzle parse(char a) {
		using swizzle = vk::ComponentSwizzle;
		if (a == 'i') return swizzle::eIdentity;
		if (a == 'z') return swizzle::eZero;
		if (a == 'o') return swizzle::eOne;
		if (a == 'r') return swizzle::eR;
		if (a == 'g') return swizzle::eG;
		if (a == 'b') return swizzle::eB;
		if (a == 'a') return swizzle::eA;
		spdlog::error("[ImageView] (from_image) unknown component swizzle {}", a);
	}
	
	ImageView ImageView::from_image(const Image& image, vk::ImageAspectFlags aspect_flags, std::string swizzle, Device &device) {

		auto type = [&]() -> std::optional<vk::ImageViewType> {
			switch (image.image_type) {
			case vk::ImageType::e1D: return vk::ImageViewType::e1D;
			case vk::ImageType::e2D: return vk::ImageViewType::e2D;
			case vk::ImageType::e3D: return vk::ImageViewType::e3D;
			default: return std::nullopt;
			}
		}();

		ovk_assert(type, "failed to get vk::ImageViewType!");

		vk::ComponentMapping mapping;
		if (!swizzle.empty()) {
			// "i" := Identitiy, "z" := zero, "o" := one, "rgba" := rgba-components
			if (swizzle.size() == 4) {
				mapping.setR(parse(swizzle.at(0)));
				mapping.setG(parse(swizzle.at(1)));
				mapping.setB(parse(swizzle.at(2)));
				mapping.setA(parse(swizzle.at(3)));
			} else {
				spdlog::error("[ImageView] (from_image) component swizzle must have 4 chars");
			}
		}
		
		vk::ImageViewCreateInfo image_view_create_info{
			{},
			image.handle.get(),
			type.value(),
			image.format,
			mapping,
			vk::ImageSubresourceRange { aspect_flags, 0, 1, 0, 1}
		};

		const auto image_view = VK_CREATE(device.device->createImageView(image_view_create_info), "Failed to create brick image view");

		return ImageView(image_view, device.device.get());
	}

	Sampler::Sampler(vk::Sampler handle, vk::Device device) : DeviceObject(device, handle) {}

}
