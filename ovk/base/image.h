#pragma once

#include "handle.h"
#include "mem.h"

namespace ovk {
	enum class QueueType;

	class OVK_API Image : public DeviceObject<vk::Image> {
		friend class Device;

		[[deprecated]]
		Image(vk::Image image, vk::DeviceMemory memory, vk::Format format, vk::Extent3D extent, vk::ImageLayout layout, vk::ImageType type, vk::Device device);

		Image(vk::ImageType type, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags flags, vk::ImageTiling tiling, mem::MemoryType mem_type, mem::Allocator* allocator, Device& device);
		
		static Image from_file_2d(const std::string& filename, vk::ImageUsageFlags image_usage, mem::Allocator* allocator, Device& device);
		static Image from_raw_data_2d(vk::Format data, uint8_t* pixels, int channels, vk::Extent3D extent, vk::ImageUsageFlags image_usage, mem::Allocator* allocator, Device& d);

	public:
		void transition_layout(vk::CommandBuffer cmd, vk::ImageLayout new_layout, QueueType queue_type, Device& device);

		// This should only be used if the image layout was changed externally (eg. from a subpass)
		// TODO: Maybe make supbasses notify the image somehow
		void set_layout(vk::ImageLayout l);
		
		std::shared_ptr<mem::View> memory;
		vk::Format format;
		vk::Extent3D extent;
		vk::ImageLayout layout;
		vk::ImageType image_type;
	};

	class OVK_API ImageView : public DeviceObject<vk::ImageView> {
		friend class Device;
		ImageView(vk::ImageView handle, vk::Device device);

		static ImageView from_image( const Image& image, vk::ImageAspectFlags aspect_flags, std::string swizzle, Device& device);
	};

	class OVK_API Sampler : public DeviceObject<vk::Sampler> {
		friend class Device;
		Sampler(vk::Sampler handle, vk::Device device);
	};
	
}
