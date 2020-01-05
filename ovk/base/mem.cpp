#include "pch.h"
#include "mem.h"

#include "device.h"

#include <sstream>

#ifdef OVK_IMGUI_UTILS
namespace ImGui {
	void MemoryBar(vk::DeviceSize size, const std::set<ovk::mem::Layout>& used_chunks, int rounding = 3);
	ImVec4 default_used_memory_color(0.2235f, 1.0f, 0.0784f, 1.0f);
	ImVec4 default_unused_memory_color(1.00f, 0.84f, 0.00f, 1.00f);
}
#endif

namespace ovk::mem {

	void debug_print_mem_types(vk::PhysicalDevice device) {
		auto memory_properties = device.getMemoryProperties();

		spdlog::debug("{:=^80}", "[ Memory Heaps: ]");
		for (auto i = 0; i < memory_properties.memoryHeapCount; i++) {
			auto heap = memory_properties.memoryHeaps[i];
			auto flags = to_string(heap.flags);  flags.pop_back(); flags.pop_back();
			spdlog::debug("heap[{}]: [ size: {}gb, flags: {} ]", i, heap.size / (pow(1000, 3)), flags);
		}
		spdlog::debug("{:=^80}", "[ Memory Types: ]");
		for (auto i = 0; i < memory_properties.memoryTypeCount; i++) {
			auto type = memory_properties.memoryTypes[i];
			auto flags = to_string(type.propertyFlags); flags.pop_back(); flags.pop_back();
			spdlog::debug("type[{}]: [ heap: {}, flags: {} ]", i, type.heapIndex, flags);
		}
		
		auto properties = device.getProperties();
		spdlog::debug("{:=^80}", "[  Alignment Info: ]");

		spdlog::debug("Buffer Image Granularity: {}b", properties.limits.bufferImageGranularity);
		spdlog::debug("Storage Buffer Offset: {}b", properties.limits.minStorageBufferOffsetAlignment);
		spdlog::debug("Uniform Buffer Offset: {}b", properties.limits.minUniformBufferOffsetAlignment);
		spdlog::debug("Texel Buffer Offset: {}b", properties.limits.minTexelBufferOffsetAlignment);

		spdlog::debug("{:=^80}", "");
	}
	
	std::optional<uint32_t> find_memory_type(vk::PhysicalDevice physical, vk::MemoryPropertyFlags memory_properties) {

		auto mem_properties = physical.getMemoryProperties();
		
		for (uint32_t index = 0; index < mem_properties.memoryTypeCount; ++index) {
			const auto flags = mem_properties.memoryTypes[index].propertyFlags;
			if ((flags & memory_properties) == memory_properties) {
				return index;
			}
		}

		return std::nullopt;
	}

	vk::MemoryPropertyFlags mem_type_to_flags(MemoryType type) {
		using flags = vk::MemoryPropertyFlagBits;
		switch (type) {
		case MemoryType::device_local:
			return flags::eDeviceLocal;
		case MemoryType::cpu_accessible:
			return flags::eHostVisible;
		case MemoryType::cpu_coherent:
			return flags::eHostVisible | flags::eHostCoherent;
		case MemoryType::cpu_cached:
			return flags::eHostVisible | flags::eHostCached;
		case MemoryType::cpu_coherent_and_cached:
			return flags::eHostVisible | flags::eHostCached | flags::eHostCoherent;
		default: return {};
		}
	}

	// ***************************************************************************************************************************
	// Memory View Types (Dedicated and Weak view at the moment (naming might (probably) will change)
	
	DedicatedView::DedicatedView(UniqueHandle<vk::DeviceMemory> &&mem, vk::DeviceSize s) : memory(std::move(mem)), size(s) {}

	vk::DeviceMemory DedicatedView::get() { return memory.get(); }
	vk::DeviceSize DedicatedView::get_offset() { return 0; }
	vk::DeviceSize DedicatedView::get_size() { return size; }

	void * DedicatedView::map(Device &device) {
		return VK_CREATE(device.device->mapMemory(memory, 0, get_size()), "[DedicatedMemory] (map) failed to map memory");
	}

	void DedicatedView::unmap(Device &device) {
		device.device->unmapMemory(memory);
	}

	DedicatedAllocator::~DedicatedAllocator() {
	}

	void * DedicatedAllocator::map(View *view, Device &device) {
		return nullptr;
	}

	void DedicatedAllocator::unmap(View *view, Device &device) {
	}

	WeakView::WeakView(vk::DeviceMemory mem, vk::DeviceSize o, vk::DeviceSize s, MemoryType t, Allocator* a) : handle(mem), offset(o), size(s), type(t), allocator(a) {}
	WeakView::~WeakView() {
		allocator->free(this);
	}

	vk::DeviceMemory WeakView::get() { return handle; }
	vk::DeviceSize WeakView::get_offset() { return offset; }
	vk::DeviceSize WeakView::get_size() { return size; }

	void* WeakView::map(Device& device) {
		return allocator->map(this, device);
	}

	void WeakView::unmap(Device& device) {
		allocator->unmap(this, device);
	}
	
	// ***************************************************************************************************************************
	// Memory Allocators

	

	std::shared_ptr<View> DedicatedAllocator::allocate(const AllocateInfo &info, ovk::Device& device) {
		using mem_prop = vk::MemoryPropertyFlagBits;
		vk::MemoryPropertyFlags memory_property;

		// TODO: This needs to be abstracted away
		if (info.type == MemoryType::cpu_accessible) memory_property = mem_prop::eHostCoherent | mem_prop::eHostVisible;
		else if (info.type == MemoryType::device_local) memory_property = mem_prop::eDeviceLocal;

		auto mem_type_index = find_memory_type(device.physical_device, memory_property);
		if (!mem_type_index || !((1 << mem_type_index.value()) & info.requirements.memoryTypeBits)) spdlog::error("[UniqueMemory] (constructor) failed to find suiting device_local memory");
		vk::MemoryAllocateInfo alloc_info{
			info.requirements.size,
			mem_type_index.value()
		};

		UniqueHandle<vk::DeviceMemory> handle(
			std::move(VK_CREATE(device.device->allocateMemory(alloc_info), "[DedicatedAllocator] (allocate) failed to allocate memory")), 
			ObjectDestroy<vk::DeviceMemory>(device.device.get()));
		return std::make_shared<DedicatedView>(std::move(handle), info.size);
		
	}

	void DedicatedAllocator::free(View *view) {
	}

	void DedicatedAllocator::debug_draw() {
		
	}

	void * MemoryBlock::map(View *view, Device &device) {
		
		if (map_count > 0) {
			map_count++;
			return static_cast<void*>(static_cast<uint8_t*>(mapped_data) + view->get_offset());
		}
		// Memory is not already mapped
		auto* data = VK_CREATE(device.device->mapMemory(handle, 0, size), "[WeakView] (map) failed to map data!");
		map_count = 1;
		mapped_data = data;
		return static_cast<void*>(static_cast<uint8_t*>(mapped_data) + view->get_offset());
		
	}
	
	void MemoryBlock::unmap(View *view, Device &device) {
		ovk_assert(map_count > 0);
		map_count--;
		if (map_count == 0) {
			device.device->unmapMemory(handle);
		}
	}

	LinearAllocator::LinearAllocator(vk::DeviceSize block_size, MemoryType type, Device& device)
		: memory{ UniqueHandle<vk::DeviceMemory>(nullptr, ObjectDestroy<vk::DeviceMemory>(device.device.get())), block_size, 0, type, nullptr, 0 },
			head(0),
			layouts() {

		const auto properties = device.physical_device.getProperties();
		const auto buffer_image_granularity = properties.limits.bufferImageGranularity;

		// TODO: Maybe we should support smaller alignments
		page_size = buffer_image_granularity;
		
		// Figure out an appropriate size
		// At the moment block size must be a multiple of image buffer granularity

		ovk_assert(block_size % buffer_image_granularity == 0, "[LinearAllocator] (constructor) block_size must be a multiple of buffer_image_granularity (for simplicity atm)");
		
		auto memory_type_index_opt =
			find_memory_type(device.physical_device, mem_type_to_flags(type));
		ovk_assert(memory_type_index_opt.has_value(), "[LinearAllocator] (constructor) failed to find suiting memory type for memory type: {}", to_string(type));

		vk::MemoryAllocateInfo alloc_info{
			block_size,
			memory_type_index_opt.value()
		};

		memory.mem_index = memory_type_index_opt.value();
		
		memory.handle.set(VK_CREATE(device.device->allocateMemory(alloc_info), "[LinearAllocator] (constructor) failed to allocate memory"));
	
	}


	vk::DeviceSize get_next_multiple(vk::DeviceSize value, vk::DeviceSize factor) {
		// is value already a multiple?
		if (value % factor == 0) return value;
		const auto result = ((value / factor) + 1) * factor;
		return result;
	}

	std::shared_ptr<View> LinearAllocator::allocate(const AllocateInfo &info, ovk::Device &device) {
		ovk_assert(memory.mem_type == info.type, "[LinearAllocator] (allocate) AllocateInfo::type({}) != LinearAllocator::memory_type({})", to_string(info.type), to_string(memory_type));
		ovk_assert(memory.mem_index & info.requirements.memoryTypeBits, "[LinearAllocator] (allocate) vk::MemoryRequirements::memoryTypeBits does not contain mem_index");


		// if non linear storage
		auto alignment = info.requirements.alignment;
		if ((uint32_t) (info.flag & AllocationFlag::non_linear)) alignment = page_size;
		auto current_page_size = get_next_multiple(info.size, info.requirements.alignment);

		if (head + current_page_size > memory.size) {
			panic("[LinearAllocator] (allocate) Allocation is to large for available memory");
			return nullptr;
		}

		auto result = std::make_shared<WeakView>(memory.handle.get(), head, current_page_size, memory.mem_type, this);

		layouts.emplace(Layout{ head , current_page_size, info.size });

		head += current_page_size;
		return result;
	}

	void LinearAllocator::free(View *view) {
		// We only support Stack like free behavior
		spdlog::info("trying to free from {} to {}", view->get_offset(), view->get_offset() + view->get_size());
		if (view->get_offset() + view->get_size() == head) {
			// Ok so this was the last allocation
			spdlog::info("[LinearAllocator] (free) Freeing from {} to {}", view->get_offset(), view->get_offset() + view->get_size());
			head -= view->get_size();
		}
	}

	

	void * LinearAllocator::map(View *view, Device &device) {
		return memory.map(view, device);
	}

	void LinearAllocator::unmap(View *view, Device &device) {
		memory.unmap(view, device);
	}

	void LinearAllocator::debug_draw() {

#ifdef OVK_IMGUI_UTILS
		ImGui::Begin("LinearAllocator");

		ImGui::MemoryBar(memory.size, layouts);

		const auto info = fmt::format("Linear Allocator: {} / {}b", head, memory.size);
		ImGui::Text(info.c_str());
		
		ImGui::End();

#endif
	}

	LayoutedMemory::LayoutedMemory(std::unique_ptr<MemoryBlock> &&memory) : memory(std::move(memory)), layout() {}

	bool intersects(const Layout& a, const Layout& b) {
		return a.offset + a.size >= b.offset && b.offset + b.size >= a.offset;
	}
	
	std::optional<Layout> LayoutedMemory::try_find(const AllocateInfo &info) {

		// O
		Layout l { 0, get_next_multiple(info.requirements.size, info.requirements.alignment), info.size };
		
		for (auto &current_layout : layout) {
			if (intersects(l, current_layout)) {
				// Try next aligned offset after the current one
				l.offset = get_next_multiple(
					current_layout.offset + current_layout.size,
					info.requirements.alignment
				);
			}
			if (l.offset + l.size > memory->size) {
				// Could not find a suiting allocation space
				return std::nullopt;
			}
		}

		layout.emplace(l);
		return l;
	}

	Pool::Pool(MemoryType type, vk::DeviceSize block_size, vk::Device device, vk::PhysicalDevice physical_device) : type(type), block_size(block_size) {
		auto index_opt = find_memory_type(physical_device, mem_type_to_flags(type));
		ovk_assert(index_opt.has_value());
		index = index_opt.value();

		add_block(device);
	}

	void Pool::add_block(vk::Device device) {

		auto vk_memory = VK_CREATE(device.allocateMemory({ block_size, index }), "[Pool] (add_block) failed to create Memory Block");
		
		auto memory = std::make_unique<MemoryBlock>(
			UniqueHandle<vk::DeviceMemory>(std::move(vk_memory), ObjectDestroy<vk::DeviceMemory>(device)),
			block_size,
			index,
			type,
			nullptr,
			0
		);

		memories.push_back(std::make_unique<LayoutedMemory>(std::move(memory)));
	}

	std::shared_ptr<View> Pool::allocate(const AllocateInfo &info, ovk::Device &device) {
		ovk_assert(info.type == type);

		auto found_place = [&]() {
			// Try to find a suiting spot in an existing memory block
			for (auto& block : memories) {
				if (auto res = block->try_find(info); res.has_value()) {
					return std::make_pair(res.value(), block->memory->handle.get());
				};
			}
			// If none is found create a new block and return that
			add_block(device.device.get());
			auto& new_block = memories[memories.size() - 1];
			auto found = new_block->try_find(info);
			ovk_assert(found.has_value());
			return std::make_pair(found.value(), new_block->memory->handle.get());
		}();

		return std::make_shared<WeakView>(found_place.second, found_place.first.offset, found_place.first.size, info.type, nullptr);
	}

	void Pool::free(View *view) {
		const auto weak_view = static_cast<WeakView*>(view);

		for (auto& memory : memories) {
			if (memory->memory->handle.get() == weak_view->handle) {
				// Remove Layout from memory bloock
				for (auto it = memory->layout.begin(); it != memory->layout.end(); ++it) {
					if (it->offset == weak_view->offset && it->size && weak_view->size) {
						memory->layout.erase(it);
						return;
					}
				}
			}
		}

		panic("not able to find that thingy!");
	}

	DefaultAllocator::DefaultAllocator(Device &ovk_device) : device(ovk_device.device.get()) {
		limits = ovk_device.physical_device.getProperties().limits;

		add_pool(MemoryType::device_local, ovk_device);
		add_pool(MemoryType::cpu_coherent, ovk_device);

	}

	DefaultAllocator::~DefaultAllocator() {
		
	}

	std::shared_ptr<View> DefaultAllocator::allocate(const AllocateInfo &info, ovk::Device &device) {

		// HACK: Should be inside try find
		AllocateInfo new_info = info;
		
		if ((uint32_t)(info.flag & AllocationFlag::non_linear))
			new_info.requirements.alignment = limits.bufferImageGranularity;
		
		if (!pools.contains(new_info.type)) {
			add_pool(new_info.type, device);
		}

		auto it = pools.find(new_info.type);

		auto view = it->second.allocate(new_info, device);
		static_cast<WeakView*>(view.get())->allocator = this;
		return view;
		
	}

	void DefaultAllocator::free(View *view) {
		const auto weak_view = static_cast<WeakView*>(view);
		auto pool_it = pools.find(weak_view->type);
		ovk_assert(pool_it != pools.end());
		pool_it->second.free(view);
	}

	void * DefaultAllocator::map(View *view, Device &device) {
		const auto weak_view =static_cast<WeakView*>(view);
		auto pool_it = pools.find(weak_view->type);
		for (auto& mem : pool_it->second.memories) {
			if (mem->memory->handle.get() == weak_view->handle) {
				return mem->memory->map(view, device);
			}
		}
		panic("could not find view memory!");
		return nullptr;
	}

	void DefaultAllocator::unmap(View *view, Device &device) {
		const auto weak_view = static_cast<WeakView*>(view);
		auto pool_it = pools.find(weak_view->type);
		for (auto& mem : pool_it->second.memories) {
			if (mem->memory->handle.get() == weak_view->handle) {
				mem->memory->unmap(view, device);
				return;
			}
		}
		panic("could not find view memory!");
	}

	void DefaultAllocator::add_pool(MemoryType type, Device& device) {

		// TODO: Might but that somewhere else
		const auto block_size = 500_mb;
		
		if (pools.find(type) == pools.end()) {
			auto [it, success] = pools.insert_or_assign(
				type,
				Pool(type, block_size, device.device.get(), device.physical_device)
			);
			ovk_assert(success);
		} else {
			panic("Tried to add a Pool that already exists!");
		}
	}

	void DefaultAllocator::debug_draw() {

#ifdef OVK_IMGUI_UTILS
		ImGui::Begin("DefaultAllocator");

		for (auto &[type, pool] : pools) {
			if (ImGui::TreeNode(to_string(type))) {
				for (auto i = 0; i < pool.memories.size(); i++) {
					if (i != 0) ImGui::Separator();
					auto& mem = pool.memories[i];
					ImGui::MemoryBar(mem->memory->size, mem->layout);
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();

#endif
		
	}

}

#ifdef OVK_IMGUI_UTILS

namespace ImGui {

	// Helpers
	ImVec2 add(ImVec2 a, ImVec2 b) {
		return ImVec2(a.x + b.x, a.y + b.y);
	}
	ImVec2 sub(ImVec2 a, ImVec2 b) {
		return ImVec2(a.x - b.x, a.y - b.y);
	}
	
	void MemoryBar(vk::DeviceSize size, const std::set<ovk::mem::Layout>& used_chunks, int rounding) {
		
		auto draw_list = ImGui::GetWindowDrawList();
		
		const auto window_pos = ImGui::GetWindowPos();
		const auto draw_start = add(window_pos, ImGui::GetCursorPos());
		const auto margin = draw_start.x - window_pos.x;
		const auto window_width = ImGui::GetWindowWidth();
		const auto height = window_width * 0.05;
		const auto draw_end = add(draw_start, ImVec2(window_width - 2 * margin, height));
		const auto length = draw_end.x - draw_start.x;

		// Background
		draw_list->AddRectFilled(draw_start, draw_end, ImGui::GetColorU32(ImGuiCol_MenuBarBg), rounding);

		vk::DeviceSize total_used = 0;
		vk::DeviceSize total_unused = 0;
		
		for (auto&& layout : used_chunks) {
			const auto start_fraction = static_cast<float>(layout.offset) / static_cast<float>(size);
			const auto used_fraction = start_fraction + static_cast<float>(layout.used_size) / static_cast<float>(size);
			const auto end_fraction = start_fraction + static_cast<float>(layout.size) / static_cast<float>(size);
			auto layout_start = add(draw_start, ImVec2(length * start_fraction, 0.0f));
			auto used_end = add(draw_start, ImVec2(length * used_fraction, height));
			auto unused_start = add(draw_start, ImVec2(length * used_fraction, 0.0f));
			auto layout_end = add(draw_start, ImVec2(length * end_fraction, height));
			draw_list->AddRectFilled(
				layout_start,
				used_end,
				ImGui::GetColorU32(default_used_memory_color),
				0,
				ImDrawCornerFlags_None
			);

			draw_list->AddRectFilled(
				unused_start,
				layout_end,
				ImGui::GetColorU32(default_unused_memory_color),
				layout.offset + layout.size == size ? rounding : 0,
				ImDrawCornerFlags_Right
			);

			total_used += layout.size;
			total_unused += layout.size - layout.used_size;

		}
		
		// Border
		draw_list->AddRect(draw_start, draw_end, ImGui::GetColorU32(ImGuiCol_Border, 3), rounding, ImDrawCornerFlags_All, 2);
		
		ImGui::Dummy(sub(draw_end, draw_start));

		const auto info_string = fmt::format("blocked: {}b / {}b | unused: {}b | allocations: {}", total_used, size, total_unused, used_chunks.size());
		const auto text_size = ImGui::CalcTextSize(info_string.c_str());

		ImGui::SetCursorPosX(window_width - text_size.x - margin);
		ImGui::Text(info_string.c_str());
		
	}
	
}

#endif
