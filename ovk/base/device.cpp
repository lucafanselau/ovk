#include "pch.h"
#include "device.h"


#include "surface.h"
#include <set>
#include <map>

#include "instance.h"
#include "swapchain.h"
#include <numeric>

namespace ovk {

	const char* to_string(QueueType e) {
		switch (e) {
		case QueueType::present: return "present";
		case QueueType::transfer: return "transfer";
		case QueueType::graphics: return "graphics";
		case QueueType::async_compute: return "async_compute";
		default: return "unknown";
		}
	}
	
	bool QueueFamilies::is_complete() const {
		return async_compute.has_value() && graphics.has_value() && present.has_value() && transfer.has_value();
	}

	QueueFamilies QueueFamilies::find(vk::PhysicalDevice ph, vk::SurfaceKHR surface) {
		QueueFamilies families;

		auto available_families = ph.getQueueFamilyProperties();
		auto i = 0;
		for (auto&& available : available_families) {

			if (available.queueCount > 0) {

				const auto present_support = VK_DCREATE(ph.getSurfaceSupportKHR(i, surface), "failed to get surface support");

				if (available.queueFlags & vk::QueueFlagBits::eGraphics) families.graphics = i;
				if (available.queueFlags & vk::QueueFlagBits::eTransfer) families.transfer = i;
				if (available.queueFlags & vk::QueueFlagBits::eCompute) families.async_compute = i;
				if (present_support) families.present = i;

				if (families.is_complete()) break;
			}
			i++;
		}

		return families;

	}

	uint32_t QueueFamilies::get_family(QueueType queue_type) {
		switch (queue_type) {
		case QueueType::present:
			return present.value();
		case QueueType::transfer:
			return transfer.value();
		case QueueType::graphics:
			return graphics.value();
		case QueueType::async_compute:
			return async_compute.value();
		default:
			assert(false);
			return 0;
		}
	}

	Device::Device(std::vector <const char*> && requested_extensions, vk::PhysicalDeviceFeatures features, Surface& s, vk::Instance* instance) {
		pick_physical(std::forward<std::vector<const char*>>(requested_extensions), features, s, instance);

		// Create Logical Device
		assert(families.is_complete());

		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
		std::set<uint32_t> unique_families = {
			families.graphics.value(),
			families.present.value(),
			families.transfer.value()
		};
		auto queue_priority = 1.0f;
		queue_create_infos.reserve(unique_families.size());
		for (auto&& family : unique_families) {
			queue_create_infos.push_back({ {}, family, 1, &queue_priority });
		}

#if defined(OVK_RENDERDOC_COMPAT)
		auto found_debug_marker_extension = false;
		// Check if it supports debug marker
		{
			auto available = VK_GET(physical_device.enumerateDeviceExtensionProperties());
			for (auto& ex : available) {
				if (!strcmp(ex.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
					spdlog::debug("adding debug marker extension");
					found_debug_marker_extension = true;
					requested_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
					break;
				}
			}
			
		}
#endif

		vk::DeviceCreateInfo create_info {
			{},
			static_cast<uint32_t>(queue_create_infos.size()),
			queue_create_infos.data(),
		#ifdef DEBUG
			static_cast<uint32_t>(validation_layers.size()),
			validation_layers.data(),
		#else
			0,
			nullptr,
		#endif
			static_cast<uint32_t>(requested_extensions.size()),
			requested_extensions.data(),
			&features
		};

		device.set(VK_CREATE(physical_device.createDevice(create_info), "failed to create device"));

		// Get Queues
		present = device->getQueue(families.present.value(), 0);
		async_compute = device->getQueue(families.async_compute.value(), 0);
		transfer = device->getQueue(families.transfer.value(), 0);
		graphics = device->getQueue(families.graphics.value(), 0);

		// Create Command Pool
		auto maybe_create_pool = [&](QueueType t) {
			if (auto family = families.get_family(t);  !command_pools.contains(family)) {
				// We need to create that Thingy
				vk::CommandPoolCreateInfo create_info{
					{},
					family
				};
				auto pool = VK_CREATE(device->createCommandPool(create_info), "Failed to create Command Pool");
				command_pools.insert(std::make_pair(family, UniqueHandle(std::move(pool), ObjectDestroy<vk::CommandPool>(device.get()))));
			}

		};

		maybe_create_pool(QueueType::present);
		maybe_create_pool(QueueType::graphics);
		maybe_create_pool(QueueType::transfer);
		maybe_create_pool(QueueType::async_compute);

		default_allocator = std::make_unique<mem::DefaultAllocator>(*this);
		
#if defined(OVK_RENDERDOC_COMPAT)
		// Load the debug marker ext functions
		if (found_debug_marker_extension) {
			debug_marker.set_object_tag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device.get(), "vkDebugMarkerSetObjectTagEXT"));
			debug_marker.set_object_name = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device.get(), "vkDebugMarkerSetObjectNameEXT"));
			debug_marker.begin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device.get(), "vkCmdDebugMarkerBeginEXT"));
			debug_marker.end = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device.get(), "vkCmdDebugMarkerEndEXT"));
			debug_marker.insert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device.get(), "vkCmdDebugMarkerInsertEXT"));

			assert(debug_marker.set_object_tag && debug_marker.set_object_name && debug_marker.begin && debug_marker.end && debug_marker.insert);
		}
#endif
		
	}

	void Device::pick_physical(std::vector<const char*> &&extensions, vk::PhysicalDeviceFeatures requested_features, Surface &s, vk::Instance* instance) {

		auto [result, devices] = instance->enumeratePhysicalDevices();
		if (devices.empty()) spdlog::error("No Physical Devices are found!");

		

		std::map<float, vk::PhysicalDevice> scores;
		for (auto&& pd : devices) {
			std::set<std::string> requested(extensions.begin(), extensions.end());
			auto available_extensions = VK_DCREATE(pd.enumerateDeviceExtensionProperties(), "failed to get physical device extension properties");
			for (auto&& available : available_extensions) {
				requested.erase(available.extensionName);
			}

			const auto found_extensions = requested.empty();
			auto indices = QueueFamilies::find(pd, s.surface);

			auto features = pd.getFeatures();
			auto properties = pd.getProperties();

			auto rf = requested_features;
			
			// Check if all requested_features are available
			bool afp = true;
			{
				if (rf.robustBufferAccess && !features.robustBufferAccess) afp = false;
				if (rf.fullDrawIndexUint32 && !features.fullDrawIndexUint32) afp = false;
				if (rf.imageCubeArray && !features.imageCubeArray) afp = false;
				if (rf.independentBlend && !features.independentBlend) afp = false;
				if (rf.geometryShader && !features.geometryShader) afp = false;
				if (rf.tessellationShader && !features.tessellationShader) afp = false;
				if (rf.sampleRateShading && !features.sampleRateShading) afp = false;
				if (rf.dualSrcBlend && !features.dualSrcBlend) afp = false;
				if (rf.logicOp && !features.logicOp) afp = false;
				if (rf.multiDrawIndirect && !features.multiDrawIndirect) afp = false;
				if (rf.drawIndirectFirstInstance && !features.drawIndirectFirstInstance) afp = false;
				if (rf.depthClamp && !features.depthClamp) afp = false;
				if (rf.depthBiasClamp && !features.depthBiasClamp) afp = false;
				if (rf.fillModeNonSolid && !features.fillModeNonSolid) afp = false;
				if (rf.depthBounds && !features.depthBounds) afp = false;
				if (rf.wideLines && !features.wideLines) afp = false;
				if (rf.largePoints && !features.largePoints) afp = false;
				if (rf.alphaToOne && !features.alphaToOne) afp = false;
				if (rf.multiViewport && !features.multiViewport) afp = false;
				if (rf.samplerAnisotropy && !features.samplerAnisotropy) afp = false;
				if (rf.textureCompressionETC2 && !features.textureCompressionETC2) afp = false;
				if (rf.textureCompressionASTC_LDR && !features.textureCompressionASTC_LDR) afp = false;
				if (rf.textureCompressionBC && !features.textureCompressionBC) afp = false;
				if (rf.occlusionQueryPrecise && !features.occlusionQueryPrecise) afp = false;
				if (rf.pipelineStatisticsQuery && !features.pipelineStatisticsQuery) afp = false;
				if (rf.vertexPipelineStoresAndAtomics && !features.vertexPipelineStoresAndAtomics) afp = false;
				if (rf.fragmentStoresAndAtomics && !features.fragmentStoresAndAtomics) afp = false;
				if (rf.shaderTessellationAndGeometryPointSize && !features.shaderTessellationAndGeometryPointSize) afp = false;
				if (rf.shaderImageGatherExtended && !features.shaderImageGatherExtended) afp = false;
				if (rf.shaderStorageImageExtendedFormats && !features.shaderStorageImageExtendedFormats) afp = false;
				if (rf.shaderStorageImageMultisample && !features.shaderStorageImageMultisample) afp = false;
				if (rf.shaderStorageImageReadWithoutFormat && !features.shaderStorageImageReadWithoutFormat) afp = false;
				if (rf.shaderStorageImageWriteWithoutFormat && !features.shaderStorageImageWriteWithoutFormat) afp = false;
				if (rf.shaderUniformBufferArrayDynamicIndexing && !features.shaderUniformBufferArrayDynamicIndexing) afp = false;
				if (rf.shaderSampledImageArrayDynamicIndexing && !features.shaderSampledImageArrayDynamicIndexing) afp = false;
				if (rf.shaderStorageBufferArrayDynamicIndexing && !features.shaderStorageBufferArrayDynamicIndexing) afp = false;
				if (rf.shaderStorageImageArrayDynamicIndexing && !features.shaderStorageImageArrayDynamicIndexing) afp = false;
				if (rf.shaderClipDistance && !features.shaderClipDistance) afp = false;
				if (rf.shaderCullDistance && !features.shaderCullDistance) afp = false;
				if (rf.shaderFloat64 && !features.shaderFloat64) afp = false;
				if (rf.shaderInt64 && !features.shaderInt64) afp = false;
				if (rf.shaderInt16 && !features.shaderInt16) afp = false;
				if (rf.shaderResourceResidency && !features.shaderResourceResidency) afp = false;
				if (rf.shaderResourceMinLod && !features.shaderResourceMinLod) afp = false;
				if (rf.sparseBinding && !features.sparseBinding) afp = false;
				if (rf.sparseResidencyBuffer && !features.sparseResidencyBuffer) afp = false;
				if (rf.sparseResidencyImage2D && !features.sparseResidencyImage2D) afp = false;
				if (rf.sparseResidencyImage3D && !features.sparseResidencyImage3D) afp = false;
				if (rf.sparseResidency2Samples && !features.sparseResidency2Samples) afp = false;
				if (rf.sparseResidency4Samples && !features.sparseResidency4Samples) afp = false;
				if (rf.sparseResidency8Samples && !features.sparseResidency8Samples) afp = false;
				if (rf.sparseResidency16Samples && !features.sparseResidency16Samples) afp = false;
				if (rf.sparseResidencyAliased && !features.sparseResidencyAliased) afp = false;
				if (rf.variableMultisampleRate && !features.variableMultisampleRate) afp = false;
				if (rf.inheritedQueries && !features.inheritedQueries) afp = false;
			}
			
			if (found_extensions && indices.is_complete() && afp) {
				auto score = 0.0f;

				// TODO: Add more checks
				if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1.f;

				if (scores.contains(score)) continue;
				scores.insert(std::make_pair(score, pd));
			}

		}

		if (scores.empty()) spdlog::info("found no device, that supports all Queues and requested Extensions and Features");
		// panic! ?
		physical_device = (--scores.end())->second;
		families = QueueFamilies::find(physical_device, s.surface);
#ifdef DEBUG
		auto properties = physical_device.getProperties();
		spdlog::debug("{:=^80}", "[ Device Information ]");
		spdlog::debug("Running on: {}:{}", properties.deviceName, properties.deviceID);
		spdlog::debug("Vulkan API: {}", properties.apiVersion);
		spdlog::debug("Driver: {}", properties.driverVersion);
		spdlog::debug("{:=^80}", "");

#endif
	}

	void Device::wait_idle() {
		VK_ASSERT(device->waitIdle(), "Failed to wait [U FUCKED UP!]");
	}

	std::optional<vk::Format> find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features, vk::PhysicalDevice device) {
		for (auto format : candidates) {
			
			auto props = device.getFormatProperties(format);
			
			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		spdlog::error("(find_supported_format) no suiting format found!");

		return std::nullopt;
	}

	std::optional<vk::Format> Device::default_depth_format() const {
		static auto depth_format = find_supported_format(
			{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment,
			physical_device
		);
		return depth_format;
	}

	SwapChain Device::create_swapchain(Surface &s) {
		return SwapChain(s, *this);
	}

	std::pair<bool, uint32_t> Device::acquire_image(SwapChain& swap_chain, vk::Semaphore signal_semaphore, vk::Fence signal_fence, uint64_t timeout) {
		uint32_t index;
		const auto result = device->acquireNextImageKHR(swap_chain.handle.get(), timeout, signal_semaphore, signal_fence, &index);
		bool recreate = false;
		
		if (result == vk::Result::eErrorOutOfDateKHR) {
			recreate = true;
		} else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			spdlog::error("Failed to acquire new image");
		}
		return std::make_pair(recreate, index);
	}

	void Device::submit(std::vector<WaitInfo> wait_semaphores, std::vector<vk::CommandBuffer> cmds, std::vector<vk::Semaphore> signal_semaphores, vk::Fence fence) {
	
		std::vector<vk::Semaphore> wait_raw_semaphores;
		std::vector<vk::PipelineStageFlags> wait_stages;
		wait_raw_semaphores.reserve(wait_semaphores.size()); wait_stages.reserve(wait_semaphores.size());
		
		for (auto&& wait : wait_semaphores) {
			wait_raw_semaphores.push_back(wait.semaphore);
			wait_stages.push_back(wait.stage);
		}
		
		vk::SubmitInfo submit_info{
			static_cast<uint32_t>(wait_semaphores.size()),
			wait_raw_semaphores.data(),
			wait_stages.data(),
			static_cast<uint32_t>(cmds.size()),
			cmds.data(),
			static_cast<uint32_t>(signal_semaphores.size()),
			signal_semaphores.data(),
		};


		VK_ASSERT(graphics.submit(1, &submit_info, fence), "Failed to submit Command Buffer");

	}

	bool Device::present_image(SwapChain &swap_chain, uint32_t index, std::vector<vk::Semaphore> wait_semaphores) {
		vk::PresentInfoKHR present_info{
			static_cast<uint32_t>(wait_semaphores.size()),
			wait_semaphores.data(),
			1,
			&swap_chain.handle.get(),
			&index
		};

		auto result = present.presentKHR(&present_info);
		if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR) {
			return true;
		}
		if (result != vk::Result::eSuccess) {
			spdlog::error("Failed to present new image");
		}
		return false;
	}

	RenderPass Device::create_render_pass(std::vector<vk::AttachmentDescription> attachments, std::vector<GraphicSubpass> subpasses, bool add_external_dependency) {
		return RenderPass(std::move(attachments), std::move(subpasses), add_external_dependency, *this);
	}

	GraphicsPipelineBuilder Device::build_pipeline() {
		return GraphicsPipelineBuilder(this);
	}

	Framebuffer Device::create_framebuffer(RenderPass& render_pass, vk::Extent3D extent, std::vector<vk::ImageView> attachments) {
		return Framebuffer(render_pass, extent, attachments, this);
	}

	mem::Allocator * Device::get_default_allocator() const {
		return default_allocator.get();
	}

	Buffer Device::create_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, void *data, std::vector<QueueType> types, mem::MemoryType mem_type,
		mem::Allocator *allocator) {
		if (!allocator) allocator = get_default_allocator();
		return Buffer(usage, size, data, types, mem_type, allocator, *this);
	}

	Buffer Device::create_staging_buffer(void *data, vk::DeviceSize size, mem::Allocator* allocator) {
		const std::vector<QueueType> queues{ QueueType::transfer };
		return Buffer(vk::BufferUsageFlagBits::eTransferSrc, size, data, queues, mem::MemoryType::cpu_accessible, allocator ? allocator : get_default_allocator(), *this);
	}
	
	Image Device::create_image_2d(const std::string &filename, vk::ImageUsageFlags image_usage) {
		return Image::from_file_2d(filename, image_usage, get_default_allocator(), *this);
	}

	Image Device::create_image_2d(vk::Format data, uint8_t *pixels, int channels, vk::Extent3D extent, vk::ImageUsageFlags image_usage) {
		return Image::from_raw_data_2d(data, pixels, channels, extent, image_usage, get_default_allocator(), *this);
	}

	Image Device::create_image(vk::ImageType type, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags flags, vk::ImageTiling tiling, mem::MemoryType mem_type, mem::Allocator* allocator) {
		if (!allocator) allocator = get_default_allocator();
		return Image(type, format, extent, flags, tiling, mem_type, allocator, *this);
	}

	ImageView Device::view_from_image(const Image &image, vk::ImageAspectFlags image_aspect, std::string swizzle) {
		return ImageView::from_image(image, image_aspect, swizzle, *this);
	}

	Sampler & Device::get_default_linear_sampler() {
		if (!default_linear_sampler) {
			vk::SamplerCreateInfo sampler_create_info{
			{},
			vk::Filter::eLinear, vk::Filter::eLinear,
			vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eRepeat,
			vk::SamplerAddressMode::eRepeat,
			vk::SamplerAddressMode::eRepeat,
			0.0f,
			VK_TRUE, 16,
			VK_FALSE, vk::CompareOp::eAlways,
			0.0f, 0.0f,
			vk::BorderColor::eIntOpaqueBlack,
			VK_FALSE
			};

			default_linear_sampler = std::unique_ptr<Sampler>(
				new Sampler(
					VK_CREATE(device->createSampler(sampler_create_info), "Failed to create sampler"), 
					device.get()
				)
			);
		}

		return *default_linear_sampler;
	}

	Sampler & Device::get_default_nearest_sampler() {
		if (!default_nearest_sampler) {
			vk::SamplerCreateInfo sampler_create_info{
			{},
			vk::Filter::eNearest, vk::Filter::eNearest,
			vk::SamplerMipmapMode::eNearest,
			vk::SamplerAddressMode::eRepeat,
			vk::SamplerAddressMode::eRepeat,
			vk::SamplerAddressMode::eRepeat,
			0.0f,
			VK_TRUE, 16,
			VK_FALSE, vk::CompareOp::eAlways,
			0.0f, 0.0f,
			vk::BorderColor::eIntOpaqueBlack,
			VK_FALSE
			};

			default_nearest_sampler = std::unique_ptr<Sampler>(
				new Sampler(
					VK_CREATE(device->createSampler(sampler_create_info), "Failed to create sampler"),
					device.get()
				)
			);
		}

		return *default_nearest_sampler;
	}


	vk::CommandBuffer Device::create_single_submit_cmd(QueueType queue_type, bool start_cmd) {

		vk::CommandBufferAllocateInfo alloc_info{
			get_command_pool(queue_type),
			vk::CommandBufferLevel::ePrimary,
			1
		};

		auto cmds = VK_CREATE(device->allocateCommandBuffers(alloc_info), "Failed to allocate Single Submit Command Buffer");
	 	if (cmds.size() != 1) spdlog::error("Failed to allocate Single Submit Command Buffer");
		auto& cmd = cmds[0];

		if (start_cmd) {
			const vk::CommandBufferBeginInfo begin {
				vk::CommandBufferUsageFlagBits::eOneTimeSubmit
			};
			VK_ASSERT(cmd.begin(begin), "Failed to begin Single Submit Command Buffer");
		}

		return cmd;

	}

	void Device::flush(vk::CommandBuffer cmd, QueueType queue, bool end, bool wait) {

		if (end) {
			VK_ASSERT(cmd.end(), "Failed to end Command Buffer");
		}

		vk::SubmitInfo submit {
			0, nullptr,
			nullptr,
			1, &cmd,
			0, nullptr
		};

		vk::Fence fence;
		if (wait) {
			fence = VK_CREATE(device->createFence({}), "Failed to create Wait Fence");
		}

		VK_ASSERT(get_queue(queue).submit(1, &submit, fence), "Failed to Submit");

		if (wait) {
			device->waitForFences(1, &fence, true, std::numeric_limits<uint64_t>::max());
			device->destroyFence(fence);

			device->freeCommandBuffers(get_command_pool(queue), { cmd });
		}

	}

	vk::Queue Device::get_queue(QueueType type) const {
		switch (type) {
		case QueueType::present: return present;
		case QueueType::transfer: return transfer;
		case QueueType::graphics: return graphics;
		case QueueType::async_compute: return async_compute;
		default: return vk::Queue();
		}
	}

	void Device::free_commands(QueueType type, std::vector<vk::CommandBuffer>& cmds) {
		device->freeCommandBuffers(get_command_pool(type), cmds);

	}

	vk::CommandPool& Device::get_command_pool(QueueType type) {
		return command_pools[families.get_family(type)].get();
	}

	DescriptorTemplateBuilder Device::build_descriptor_template() {
		return DescriptorTemplateBuilder(*this);
	}

	DescriptorPool Device::create_descriptor_pool(std::vector<DescriptorTemplate*> sets, std::vector<uint32_t> num_sets) {

		if (sets.size() != num_sets.size()) spdlog::error("[Device] (create_descriptor_pool) For every template u need to specify a number");

		std::vector<vk::DescriptorPoolSize> sizes;

		uint32_t buffer_size = 0, combined_sampler_size = 0;
		for (auto i = 0; i < sets.size(); i++) {
			for (auto& info : sets[i]->infos) {
				if (info.type == vk::DescriptorType::eUniformBuffer) buffer_size += num_sets[i];
				if (info.type == vk::DescriptorType::eCombinedImageSampler) combined_sampler_size += num_sets[i];
			}
		}

		if (buffer_size > 0) sizes.emplace_back(vk::DescriptorType::eUniformBuffer, buffer_size);
		if (combined_sampler_size > 0) sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, combined_sampler_size);

		vk::DescriptorPoolCreateInfo create_info{
			{},
			std::accumulate(num_sets.begin(), num_sets.end(), static_cast<uint32_t>(0)),
			static_cast<uint32_t>(sizes.size()),
			sizes.data()
		};

		const auto raw = VK_CREATE(device->createDescriptorPool(create_info), "Failed to create Descriptor Pool");
		
		return DescriptorPool(raw, device.get());
	}

	std::vector<DescriptorSet> Device::make_descriptor_sets(DescriptorPool& pool, uint32_t count, const DescriptorTemplate& descriptor_template) {
		std::vector<vk::DescriptorSetLayout> layouts(count, descriptor_template.handle.get());
		vk::DescriptorSetAllocateInfo alloc_info{
			pool.handle.get(),
			count,
			layouts.data()
		};
		auto raw_sets = VK_CREATE(device->allocateDescriptorSets(alloc_info), "[Device] (make_descriptor_sets) Failed to allocate Raw Handles!");
		std::vector<DescriptorSet> sets;
		for (uint32_t i = 0; i < count; i++) {
			sets.push_back(DescriptorSet(raw_sets[i], device.get()));
		}
		return sets;
	}

	std::vector<Fence> Device::create_fences(uint32_t count, vk::FenceCreateFlags flags) {
		std::vector<Fence> result;
		result.reserve(count);
		for (auto i = 0; i < count; i++) {
			result.push_back(Fence(flags, device.get()));
		}
		return result;
	}

	std::vector<Semaphore> Device::create_semaphores(uint32_t count) {
		std::vector<Semaphore> result;
		result.reserve(count);
		for (auto i = 0; i < count; i++) {
			result.push_back(Semaphore(device.get()));
		}
		return result;
	}

	bool Device::wait_fences(std::vector<vk::Fence> fences, bool wait_all, uint64_t timeout) {
		auto result = device->waitForFences(fences, wait_all, timeout);
		if (!(result == vk::Result::eSuccess || result == vk::Result::eTimeout))
			spdlog::error("Failed to wait for fences");
		return result == vk::Result::eSuccess;
	}

	void Device::reset_fences(std::vector<vk::Fence> fences) {
		device->resetFences(fences);
	}

}
