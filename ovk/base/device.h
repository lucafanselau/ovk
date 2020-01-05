#pragma once
#include "handle.h"
#include <optional>

#include "swapchain.h"
#include "render_pass.h"
#include "pipeline.h"
#include "framebuffer.h"
#include "buffer.h"
#include "render_command.h"
#include "descriptor.h"
#include "debug.h"
#include "sync.h"
#include "image.h"

namespace ovk {
	class Surface;

	
	enum class QueueType {
		present,
		transfer,
		graphics,
		async_compute
	};


	struct QueueFamilies {
		std::optional<uint32_t> present, transfer, graphics, async_compute;

		bool is_complete() const;
		static QueueFamilies find(vk::PhysicalDevice ph, vk::SurfaceKHR surface);

		uint32_t get_family(QueueType queue_type);
	};

	
	
	/**
	 * \brief Picks Suiting Physical Device and Creates Logical Device on-top
	 */
	class OVK_API Device {
	private:
		Device(std::vector<const char*>&& requested_extensions, vk::PhysicalDeviceFeatures requested_features, Surface& s, vk::Instance* instance);
		friend class Instance;
		void pick_physical(std::vector<const char*>&& extensions, vk::PhysicalDeviceFeatures requested_features, Surface& s, vk::Instance* instance);
	public:

		Device(Device&& other) = default;
		Device(const Device& other) = delete;

		// ***************************************************************************************************************************************************************
		// Utils

		void wait_idle();
		std::optional<vk::Format> default_depth_format() const;
		
		// ***************************************************************************************************************************************************************
		// Swapchain
		
		SwapChain create_swapchain(Surface& s);

		std::pair<bool, uint32_t> acquire_image(SwapChain& swap_chain, vk::Semaphore signal_semaphore = {}, vk::Fence signal_fence = {}, uint64_t timeout = VK_STD_TIMEOUT);
		void submit(std::vector<WaitInfo> wait_semaphores, std::vector<vk::CommandBuffer> cmds, std::vector<vk::Semaphore> signal_semaphores, vk::Fence fence = {});

		bool present_image(SwapChain& swap_chain, uint32_t index, std::vector<vk::Semaphore> wait_semaphores);
		
		// ***************************************************************************************************************************************************************
		// Render Pass
		
		RenderPass create_render_pass(std::vector<vk::AttachmentDescription> attachments, std::vector<GraphicSubpass> subpasses, bool add_external_dependency);

		// ***************************************************************************************************************************************************************
		// Pipelines

		GraphicsPipelineBuilder build_pipeline();

		// ***************************************************************************************************************************************************************
		// Framebuffers
		
		Framebuffer create_framebuffer(RenderPass& render_pass, vk::Extent3D extent, std::vector<vk::ImageView> attachments);

		// ***************************************************************************************************************************************************************
		// Regular Buffers

		mem::Allocator* get_default_allocator() const;
		
		template<typename T>
		Buffer create_vertex_buffer(const std::vector<T>& data, mem::MemoryType type, mem::Allocator* allocator = nullptr);

		template <typename T, size_t S>
		Buffer create_vertex_buffer(const std::array<T, S>& data, mem::MemoryType type, mem::Allocator* allocator = nullptr);

		template<typename T>
		Buffer create_index_buffer(const std::vector<T>& data, mem::MemoryType type, mem::Allocator* allocator = nullptr);

		template<typename T, size_t S>
		Buffer create_index_buffer(const std::array<T, S>& data, mem::MemoryType type, mem::Allocator* allocator = nullptr);

		Buffer create_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, void* data, std::vector<QueueType> types, mem::MemoryType mem_type, mem::Allocator* allocator = nullptr);
		
		template <typename T>
		Buffer create_uniform_buffer(T& data, mem::MemoryType type, std::vector<QueueType> queues, mem::Allocator* allocator = nullptr);

		Buffer create_staging_buffer(void* data, vk::DeviceSize size, mem::Allocator* allocator = nullptr);
		
		template <typename T>
		void update_buffer(Buffer& buffer, T& data);
		// ***************************************************************************************************************************************************************
		// Images

		Image create_image_2d(const std::string& filename, vk::ImageUsageFlags image_usage);
		Image create_image_2d(vk::Format data, uint8_t* pixels, int channels, vk::Extent3D extent, vk::ImageUsageFlags image_usage);

		Image create_image(vk::ImageType type, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags flags, vk::ImageTiling tiling, mem::MemoryType mem_type, mem::Allocator* allocator = nullptr);
		
		ImageView view_from_image(const Image& image, vk::ImageAspectFlags image_aspect = vk::ImageAspectFlagBits::eColor, std::string swizzle = "");

		Sampler& get_default_linear_sampler();
		Sampler& get_default_nearest_sampler();
		
		// ***************************************************************************************************************************************************************
		// Command Buffers

		template <typename Lambda>
		std::vector<RenderCommand> create_render_commands(size_t count, Lambda&& init_capture);

		vk::CommandBuffer create_single_submit_cmd(QueueType queue_type, bool start_cmd = true);
		void flush(vk::CommandBuffer cmd, QueueType queue, bool end, bool wait);

		// Note: Maybe abstract render_commands one layer up so it could be more generalized
		void free_commands(QueueType type, std::vector<vk::CommandBuffer>& cmds);
		
		[[nodiscard]] vk::CommandPool& get_command_pool(QueueType type);
		
		// ***************************************************************************************************************************************************************
		// Queue
		
		[[nodiscard]] vk::Queue get_queue(QueueType type) const;
		

		// ***************************************************************************************************************************************************************
		// Descriptors
		DescriptorTemplateBuilder build_descriptor_template();
		DescriptorPool create_descriptor_pool(std::vector<DescriptorTemplate*> sets, std::vector<uint32_t> num_sets);

		std::vector<DescriptorSet> make_descriptor_sets(DescriptorPool& pool, uint32_t count, const DescriptorTemplate& descriptor_template);

		// ***************************************************************************************************************************************************************
		// Sync Functions
		std::vector<Fence> create_fences(uint32_t count, vk::FenceCreateFlags flags = {});
		std::vector<Semaphore> create_semaphores(uint32_t count);

		bool wait_fences(std::vector<vk::Fence> fences, bool wait_all = true, uint64_t timeout = VK_STD_TIMEOUT);
		void reset_fences(std::vector<vk::Fence> fences);


		// ***************************************************************************************************************************************************************
		// Fields
		UniqueHandle<vk::Device> device;
		vk::PhysicalDevice physical_device;
		QueueFamilies families;

		vk::Queue present, transfer, graphics, async_compute;
		std::unordered_map<uint32_t, UniqueHandle<vk::CommandPool>> command_pools;
		
	private:
		std::unique_ptr<Sampler> default_linear_sampler = nullptr;
		std::unique_ptr<Sampler> default_nearest_sampler = nullptr;

		std::unique_ptr<mem::DefaultAllocator> default_allocator = nullptr;
	public:
		// ***************************************************************************************************************************************************************
		// Debug Marker
		
#if defined(OVK_RENDERDOC_COMPAT)
		struct {
			PFN_vkDebugMarkerSetObjectTagEXT set_object_tag = nullptr;
			PFN_vkDebugMarkerSetObjectNameEXT set_object_name = nullptr;
			PFN_vkCmdDebugMarkerBeginEXT begin = nullptr;
			PFN_vkCmdDebugMarkerEndEXT end = nullptr;
			PFN_vkCmdDebugMarkerInsertEXT insert = nullptr;
		} debug_marker;
#endif

		template <typename T>
		void set_name(T& obj, const std::string& name);
		
		template <typename T, typename... Args>
		void set_name(T& obj, const char* fmt, const Args& ... args);

		template <typename T, typename Tag>
		void set_tag(T& obj, Tag& data);
		
	};
	

	template <typename T>
	Buffer Device::create_vertex_buffer(const std::vector<T> &data, mem::MemoryType type, mem::Allocator* allocator) {
		std::vector<QueueType> queues{ QueueType::graphics };
		return Buffer(vk::BufferUsageFlagBits::eVertexBuffer, sizeof(T) * data.size(), (void*) data.data(), queues, type, allocator ? allocator : get_default_allocator(), *this);
	}

	template <typename T, size_t S>
	Buffer Device::create_vertex_buffer(const std::array<T, S> &data, mem::MemoryType type, mem::Allocator* allocator) {
		std::vector<QueueType> queues{ QueueType::graphics };
		return Buffer(vk::BufferUsageFlagBits::eVertexBuffer, sizeof(T) * data.size(), (void*)data.data(), queues, type, allocator ? allocator : get_default_allocator(), *this);
	}

	template <typename T>
	Buffer Device::create_index_buffer(const std::vector<T> &data, mem::MemoryType type, mem::Allocator* allocator) {
		std::vector<QueueType> queues{ QueueType::graphics };
		return Buffer(vk::BufferUsageFlagBits::eIndexBuffer, sizeof(T) * data.size(), (void*)data.data(), queues, type, allocator ? allocator : get_default_allocator(), *this);
		
	}

	template <typename T, size_t S>
	Buffer Device::create_index_buffer(const std::array<T, S> &data, mem::MemoryType type, mem::Allocator* allocator) {
		std::vector<QueueType> queues{ QueueType::graphics };
		return Buffer(vk::BufferUsageFlagBits::eIndexBuffer, sizeof(T) * data.size(), (void*)data.data(), queues, type, allocator ? allocator : get_default_allocator(), *this);

	}

	template <typename T>
	Buffer Device::create_uniform_buffer(T &data, mem::MemoryType type, std::vector<QueueType> queues, mem::Allocator* allocator) {
		return Buffer(vk::BufferUsageFlagBits::eUniformBuffer, sizeof(T), static_cast<void*>(&data), queues, type, allocator ? allocator : get_default_allocator(), *this);
	}

	template <typename T>
	void Device::update_buffer(Buffer &buffer, T &data) {
		buffer.upload(sizeof(T), static_cast<void*>(&data), *this);
	}


	template <typename Lambda>
	std::vector<RenderCommand> Device::create_render_commands(size_t count, Lambda &&init_capture) {

		static_assert(std::is_invocable_v<Lambda, RenderCommand&, int>, "Lambda must be invocable with (RenderCommand&, int)");

		vk::CommandBufferAllocateInfo alloc_info{
		get_command_pool(ovk::QueueType::graphics),
		vk::CommandBufferLevel::ePrimary,
		static_cast<uint32_t>(count)
		};


		auto command_buffers = VK_CREATE(device->allocateCommandBuffers(alloc_info), "Failed to create Rendering Command Buffers");

		std::vector<RenderCommand> render_commands;

		for (int i = 0; i < command_buffers.size(); i++) {
			vk::CommandBuffer& cmd_handle = command_buffers[i];


			auto it = render_commands.size();
			render_commands.push_back(RenderCommand(cmd_handle, *this));


			auto& cmd = render_commands[it];
			cmd.start();

			init_capture(cmd, i);

			cmd.end();

			// After recording remove reference to this class
			cmd.device = nullptr;
			
		}

		return render_commands;

	}

#ifdef OVK_RENDERDOC_COMPAT
	
	template <typename T>
	void Device::set_name(T &obj, const std::string& name) {
#ifdef RELEASE
		return;
#endif
		if (!debug_marker.set_object_name) return;
		Tagger<T>::set_name(obj, name, device.get(), debug_marker.set_object_name);
	}

 	template <typename T, typename ... Args>
	void Device::set_name(T &obj, const char *fmt, const Args &... args) {
#ifdef RELEASE
		return;
#endif
		if (!debug_marker.set_object_name) return;
		std::string name = fmt::vformat(fmt, fmt::make_format_args(args...));
		set_name<T>(obj, name);
	}

	template <typename T, typename Tag>
	void Device::set_tag(T &obj, Tag &data) {
#ifdef RELEASE
		return;
#endif
		if (!debug_marker.set_object_tag) return;
		Tagger<T>::set_tag(obj, data, device.get(), debug_marker.set_object_tag);
	}

	// tagger partial specializations
	template <>
	struct TagInfo<Buffer> {
		static vk::DebugReportObjectTypeEXT get_type() { return vk::DebugReportObjectTypeEXT::eBuffer; }
		static uint64_t get_handle(Buffer& buffer) { return (uint64_t)(VkBuffer)buffer.handle.get(); }
	};

	template <>
	struct TagInfo<Framebuffer> {
		static vk::DebugReportObjectTypeEXT get_type() { return vk::DebugReportObjectTypeEXT::eFramebuffer; }
		static uint64_t get_handle(Framebuffer& framebuffer) { return (uint64_t)(VkFramebuffer) framebuffer.handle.get(); }
	};

#else

	template <typename T>
	void Device::set_name(T &obj, const std::string &name) {
	}

	template <typename T, typename ... Args>
	void Device::set_name(T &obj, const char *fmt, const Args &... args) {
	}

	template <typename T, typename Tag>
	void Device::set_tag(T &obj, Tag &data) {
	}
	
#endif
	

}
