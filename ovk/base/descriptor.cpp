#include "pch.h"
#include "descriptor.h"

#include "device.h"

namespace ovk {



  DescriptorTemplateBuilder & DescriptorTemplateBuilder::add_uniform_buffer(uint32_t binding, vk::ShaderStageFlags stage) {
    descriptor::Info info{ binding, vk::DescriptorType::eUniformBuffer, stage };
		infos.push_back(info);
		return *this;
  }

  DescriptorTemplateBuilder & DescriptorTemplateBuilder::add_dynamic_uniform_buffer(uint32_t binding, vk::ShaderStageFlags stage) {
    descriptor::Info info{ binding, vk::DescriptorType::eUniformBufferDynamic, stage };
		infos.push_back(info);
		return *this;
  }

	DescriptorTemplateBuilder & DescriptorTemplateBuilder::add_sampler(uint32_t binding, vk::ShaderStageFlags stage) {
		infos.push_back(descriptor::Info{ binding, vk::DescriptorType::eCombinedImageSampler, stage });
		return *this;
	}

	DescriptorTemplate DescriptorTemplateBuilder::build() {

		std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
		for (auto& info : infos) layout_bindings.emplace_back(info.binding, info.type, 1, info.stage, nullptr);

		vk::DescriptorSetLayoutCreateInfo create_info{
			{},
			static_cast<uint32_t>(layout_bindings.size()),
			layout_bindings.data()
		};

		auto raw_handle = VK_CREATE(device.createDescriptorSetLayout(create_info), "Failed to create Descriptor Set Layout");

		return DescriptorTemplate(raw_handle, infos, device);
	}

	DescriptorTemplateBuilder::DescriptorTemplateBuilder(Device &d)
		: device(d.device.get()) {}

	DescriptorTemplate::DescriptorTemplate(vk::DescriptorSetLayout raw, std::vector<descriptor::Info> &infos, vk::Device device)
		: DeviceObject<vk::DescriptorSetLayout>(device, raw),
			infos(infos) {}

	DescriptorPool::DescriptorPool(vk::DescriptorPool raw, vk::Device device) : DeviceObject<vk::DescriptorPool>(device, raw) {}

	DescriptorSet::DescriptorSet(vk::DescriptorSet raw, vk::Device device) : set(raw), device(device) {}

  void DescriptorSet::write(Buffer &buffer, uint32_t offset, uint32_t binding, bool dynamic) const {
    vk::DescriptorBufferInfo buffer_info{
			buffer.handle.get(),
			offset,
		  VK_WHOLE_SIZE
		};

		const vk::WriteDescriptorSet write_descriptor{
			set,
			binding,
			0,
			1,
			dynamic ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer,
			nullptr,
			&buffer_info
		};

		device.updateDescriptorSets({ write_descriptor }, {});
  }

  void DescriptorSet::write(vk::Sampler sampler, vk::ImageView view, vk::ImageLayout layout, uint32_t binding) {
		vk::DescriptorImageInfo image_info {
			sampler, view, layout
		};

		const vk::WriteDescriptorSet write_descriptor{
			set,
			binding,
			0,
			1,
			vk::DescriptorType::eCombinedImageSampler,
			&image_info,			
		};

		device.updateDescriptorSets({ write_descriptor }, {});
	}

	DescriptorSet::operator vk::DescriptorSet() const { return set; }



}
