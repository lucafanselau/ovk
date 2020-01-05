#pragma once

#include "handle.h"

#include "buffer.h"

namespace ovk {
	class Buffer;

	class Device;
	class DescriptorTemplate;
	
	namespace descriptor {
		struct OVK_API Info {
			uint32_t binding;
			vk::DescriptorType type;
			vk::ShaderStageFlags stage;
		};
	}

	struct OVK_API DescriptorTemplateBuilder {

		DescriptorTemplateBuilder& add_uniform_buffer(uint32_t binding, vk::ShaderStageFlags stage);

		DescriptorTemplateBuilder& add_dynamic_uniform_buffer(uint32_t binding, vk::ShaderStageFlags stage);
		
		DescriptorTemplateBuilder& add_sampler(uint32_t binding, vk::ShaderStageFlags stage);
		
		DescriptorTemplate build();
		
	private:
		friend Device;
		explicit DescriptorTemplateBuilder(Device& device);

		vk::Device device;
		std::vector<descriptor::Info> infos;
		
	};

	class OVK_API DescriptorTemplate : public DeviceObject<vk::DescriptorSetLayout> {

		friend DescriptorTemplateBuilder;
		DescriptorTemplate(vk::DescriptorSetLayout raw, std::vector<descriptor::Info>& infos, vk::Device device);

	public:
		std::vector<descriptor::Info> infos;
		
	};

	class OVK_API DescriptorPool : public DeviceObject<vk::DescriptorPool> {
		friend Device;
		DescriptorPool(vk::DescriptorPool raw, vk::Device device);
	};

	class OVK_API DescriptorSet {
		friend Device;
		DescriptorSet(vk::DescriptorSet raw, vk::Device device);
	public:

		void write(Buffer& buffer, uint32_t offset, uint32_t binding, bool dynamic = false) const;

		void write(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout, uint32_t binding);
		
		OVK_CONVERSION operator vk::DescriptorSet() const;

		vk::DescriptorSet set;
		vk::Device device;
	};

}
