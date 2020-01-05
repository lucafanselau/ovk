#pragma once

#include "handle.h"
#include <imgui.h>
#include <set>

namespace ovk {
	class Device;
}

namespace ovk::mem {

	constexpr vk::DeviceSize operator ""_mb(vk::DeviceSize a) {
		return a * 1024 * 1024;
	}

	constexpr vk::DeviceSize operator ""_kb(vk::DeviceSize a) {
		return a * 1024;
	}

	// TODO: This does not represent all available types of memory vulkan has to offer
	// See (debug_print_mem_types(vk::PhysicalDevivce))
	enum class OVK_API MemoryType {
		device_local,
		cpu_accessible,
		cpu_coherent,
		cpu_cached,
		cpu_coherent_and_cached
	};

	inline const char* to_string(MemoryType e) {
		switch (e) {
		case MemoryType::device_local: return "device_local";
		case MemoryType::cpu_accessible: return "cpu_accessible";
		case MemoryType::cpu_coherent: return "cpu_coherent";
		case MemoryType::cpu_cached: return "cpu_cached";
		case MemoryType::cpu_coherent_and_cached: return "cpu_coherent_and_cached";
		default: return "unknown";
		}
	}

	OVK_API void debug_print_mem_types(vk::PhysicalDevice device);
	
	OVK_API std::optional<uint32_t> find_memory_type(vk::PhysicalDevice physical, vk::MemoryPropertyFlags memory_properties);
	OVK_API vk::MemoryPropertyFlags mem_type_to_flags(MemoryType type);
	
	struct OVK_API View {
		virtual ~View() = default;
		virtual vk::DeviceMemory get() = 0;
		virtual vk::DeviceSize get_offset() = 0;
		virtual vk::DeviceSize get_size() = 0;

		virtual void* map(Device& device) = 0;
		virtual void unmap(Device& device) = 0;
	};
	
	enum class AllocationFlag : uint32_t {
		none = 0,
		non_linear = 1 << 0
	};
	constexpr AllocationFlag operator|(AllocationFlag a, AllocationFlag b) {
		return a = static_cast<AllocationFlag> (a | b);
	}

	constexpr AllocationFlag operator&(AllocationFlag a, AllocationFlag b) {
		return a = static_cast<AllocationFlag> (static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}
	
	constexpr AllocationFlag& operator|=(AllocationFlag& a, AllocationFlag b) {
		return a = static_cast<AllocationFlag> (static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	struct OVK_API AllocateInfo {
		// MemoryType might be invalid usage when using certain Allocator (ovk will panic!)
		MemoryType type;
		uint32_t size;
		AllocationFlag flag = AllocationFlag::none;
		vk::MemoryRequirements requirements;
	};
	
	struct OVK_API Allocator {
		Allocator() = default;
		virtual ~Allocator() = default;

		Allocator(const Allocator &other) = delete;
		Allocator(Allocator &&other) noexcept = delete;
		Allocator & operator=(const Allocator &other) = delete;
		Allocator & operator=(Allocator &&other) noexcept = delete;
		
		virtual std::shared_ptr<View> allocate(const AllocateInfo& info, ovk::Device& device) = 0;
		virtual void free(View* view) = 0;

		virtual void* map(View* view, Device& device) = 0;
		virtual void unmap(View* view, Device& device) = 0;

		virtual void debug_draw() = 0;
	};

	// TODO: Could be replaced with Weak View and therefore we don't need polymorphism here
	struct OVK_API DedicatedView : View {

		DedicatedView(UniqueHandle<vk::DeviceMemory>&& mem, vk::DeviceSize s);
		
		UniqueHandle<vk::DeviceMemory> memory;
		vk::DeviceSize size;
		~DedicatedView() override = default;
		vk::DeviceMemory get() override;
		vk::DeviceSize get_offset() override;
		vk::DeviceSize get_size() override;
		void * map(Device &device) override;
		void unmap(Device &device) override;
	};

	
	struct OVK_API WeakView : View {

		WeakView(vk::DeviceMemory mem, vk::DeviceSize o, vk::DeviceSize s, MemoryType t, Allocator* a);
		
		vk::DeviceMemory handle;
		vk::DeviceSize offset, size;
		MemoryType type;

		Allocator* allocator;

		~WeakView() override;
		vk::DeviceMemory get() override;
		vk::DeviceSize get_offset() override;
		vk::DeviceSize get_size() override;
		void * map(Device &device) override;
		void unmap(Device &device) override;
	};
	
	struct OVK_API DedicatedAllocator : Allocator {
		std::shared_ptr<View> allocate(const AllocateInfo &info, ovk::Device& device) override;
		void free(View *view) override;
		~DedicatedAllocator() override;
		void * map(View *view, Device &device) override;
		void unmap(View *view, Device &device) override;
		void debug_draw() override;
	};

	// Memory Handle plus some Meta Data
	struct MemoryBlock {

		MemoryBlock(UniqueHandle<vk::DeviceMemory> &&handle, vk::DeviceSize size, uint32_t mem_index, MemoryType mem_type, void *mapped_data, uint32_t map_count)
			: handle(std::move(handle)),
			  size(size),
			  mem_index(mem_index),
			  mem_type(mem_type),
			  mapped_data(mapped_data),
			  map_count(map_count) {
		}

		void* map(View* view, Device& device);
		void unmap(View* view, Device& device);


		UniqueHandle<vk::DeviceMemory> handle;
		vk::DeviceSize size;
		uint32_t mem_index;
		MemoryType mem_type;
		void* mapped_data = nullptr; // NOTE: The Allocator is expected to do reference counting on this
		uint32_t map_count = 0;
	};

	struct OVK_API Layout {

		vk::DeviceSize offset, size, used_size;

		friend bool operator==(const Layout& lhs, const Layout& rhs) {
			return lhs.offset == rhs.offset
				&& lhs.size == rhs.size
				&& lhs.used_size == rhs.used_size;
		}

		friend bool operator!=(const Layout& lhs, const Layout& rhs) {
			return !(lhs == rhs);
		}

		friend bool operator<(const Layout& lhs, const Layout& rhs) { return lhs.offset < rhs.offset; }
		friend bool operator<=(const Layout& lhs, const Layout& rhs) { return !(rhs < lhs); }
		friend bool operator>(const Layout& lhs, const Layout& rhs) { return rhs < lhs; }
		friend bool operator>=(const Layout& lhs, const Layout& rhs) { return !(lhs < rhs); }
	};
	
	struct OVK_API LinearAllocator : Allocator {

		MemoryBlock memory;
		
		vk::DeviceSize head;
		vk::DeviceSize page_size;

		// TODO: 
		std::set<Layout> layouts;
		
		LinearAllocator(vk::DeviceSize block_size, MemoryType type, Device& device);
		~LinearAllocator() override = default;
		std::shared_ptr<View> allocate(const AllocateInfo &info, ovk::Device &device) override;
		void free(View *view) override;
		void * map(View *view, Device &device) override;
		void unmap(View *view, Device &device) override;
		void debug_draw() override;
	};

	struct OVK_API LayoutedMemory {
		LayoutedMemory() = default;
		LayoutedMemory(std::unique_ptr<MemoryBlock>&& memory);
		
		LayoutedMemory(const LayoutedMemory &other) = delete;
		LayoutedMemory(LayoutedMemory &&other) noexcept = default;
		LayoutedMemory & operator=(const LayoutedMemory &other) = delete;
		LayoutedMemory & operator=(LayoutedMemory &&other) noexcept = default;

		~LayoutedMemory() = default;

		std::optional<Layout> try_find(const AllocateInfo& info);
		
		std::unique_ptr<MemoryBlock> memory = nullptr;
		std::set<Layout> layout = {};
	};
	
	struct OVK_API Pool {
		Pool(MemoryType type, vk::DeviceSize block_size, vk::Device device, vk::PhysicalDevice physical_device);

		Pool(const Pool &other) = delete;
		Pool(Pool &&other) noexcept = default;
		Pool & operator=(const Pool &other) = delete;
		Pool & operator=(Pool &&other) noexcept = default;

		~Pool() = default;
		
		void add_block(vk::Device device);
		std::shared_ptr<View> allocate(const AllocateInfo& info, ovk::Device& device);
		void free(View* view);

		std::vector<std::unique_ptr<LayoutedMemory>> memories;
		MemoryType type;
		uint32_t index;
		vk::DeviceSize block_size;
	};

	struct OVK_API DefaultAllocator : Allocator {
		explicit DefaultAllocator(Device& device);
		~DefaultAllocator() override;
		std::shared_ptr<View> allocate(const AllocateInfo &info, ovk::Device &device) override;
		void free(View *view) override;

		void * map(View *view, Device &device) override;
		void unmap(View *view, Device &device) override;
		
		void add_pool(MemoryType type, Device& device);
		
		// members:
		vk::Device device;
		vk::PhysicalDeviceLimits limits;
		std::unordered_map<MemoryType, Pool> pools;
		
		void debug_draw() override;
	};

}
