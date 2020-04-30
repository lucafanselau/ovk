#pragma once

#include "def.h"

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#include <spdlog/spdlog.h>

#include <cassert>
#include <type_traits>
#include <GLFW/glfw3.h>
#include <optional>

namespace ovk {
	class Device;

	// Example Usage UniqueHandle with custom Deallocator
	//
	// struct IntDeallocator {
	//
	// 	void operator()(int& data) {
	// 		vec.push_back(data);
	// 	}
	//
	// 	~IntDeallocator() {
	// 		std::stringstream ss;
	// 		ss << "[";
	// 		for (auto&& i : vec) {
	// 			ss << " " << i;
	// 		}
	// 		spdlog::info("Deleted: {}", ss.str());
	// 	}
	//
	// 	std::vector<int> vec;
	//
	// };
	//
	// ... [Somewhere in ur code]
	// ovk::UniqueHandle<int> my_handle(2, [](int& data) {
	// 	spdlog::info("Data got deleteeddd: {}", data);
	// });
	//
	// ovk::UniqueHandle<int> my_handle2(3, [](int& data) {
	// 	spdlog::info("Data is gone: {}", data);
	// });
	//
	// my_handle2 = std::move(my_handle);
	//
	// IntDeallocator mass_deallocator;
	//
	// for (int i = 0; i < 10; i++) {
	// 	ovk::UniqueHandle<int, IntDeallocator> handle(i, mass_deallocator);
	// }

	struct NoDeallocator {};

	template <typename T>
	struct ObjectDestroy {
		void operator()(T& data) = delete;
	};

	/**
	 * \brief This is a unique (e.g. non copyable) handle to some data
	 * \tparam T The Object type of the Handle
	 * \tparam Deallocator Callable Type that can be used to add a deleting Callback (eg. vk::Destroy[...]()) or be of type NoDeallocator (then u would need to pass a NoDeallocator instance to all constructors)
	 */
	template <typename T, typename Deallocator = ObjectDestroy<T>>
	struct UniqueHandle {

		static_assert(std::is_copy_constructible<T>::value, "[UniqueHandle] T must be copy constructable");
		static_assert(std::is_same<Deallocator, ObjectDestroy<T>>::value || std::is_invocable<Deallocator, T&>::value || std::is_same<Deallocator, NoDeallocator>::value, "[UniqueHandle] Deallocator must be of type ObjectDestroy<T>, invocable with T& or be NoDeallocator");

		/**
		 * \brief This Constructor constructs a Unique Handle that is invalid
		 */
		UniqueHandle() : data(nullptr), should_delete_deallocator(false), deallocator() {}

		/**
		 * \brief This Constructs a Handle that already holds Data inside of it
		 *				NOTE: This might lead to a performance impact because of the Copy Construction on the Heap
		 * \param raw_data r-value reference to the to be copied data
		 * \param d The Deallocator to be called on the Data if this Handle is valid at deconstruction [This Version copies the Deallocator]
		 */
		UniqueHandle(T&& raw_data, Deallocator&& d) :
			data(nullptr),
			should_delete_deallocator(true),
			deallocator(new Deallocator(std::forward<Deallocator>(d))) {
			// Copy Raw Data into the Data Pointer
			set(std::forward<T>(raw_data));
		}

		/**
		 * \brief This Constructs a Handle that already holds Data inside of it
		 *				NOTE: This might lead to a performance impact because of the Copy Construction on the Heap
		 * \param raw_data r-value reference to the to be copied data
		 * \param d The Deallocator to be called on the Data if this Handle is valid at deconstruction [This Version references an existing Deallocator]
		 */
		UniqueHandle(T&& raw_data, Deallocator& d) :
			data(nullptr),
			should_delete_deallocator(false),
			deallocator(&d) {
			// Copy Raw Data into the Data Pointer
			set(std::forward<T>(raw_data));
		}


		/**
		 * \brief This Constructs a Handle that already holds Data inside of it from an Unmanaged Pointer
		 *				NOTE: This might lead to a performance impact because of the Copy Construction on the Heap
		 * \param raw_data Independent Pointer to a Resource (If Pointer Lifetime is limited DO NOT USE!!!!) 
		 * \param d Deallocator
		 */
		UniqueHandle(T* raw_data, Deallocator&& d) :
			data(raw_data),
			should_delete_deallocator(true),
			deallocator(new Deallocator(std::forward<Deallocator>(d))) {}

		/**
		 * \brief Disallow Copy Constructing since the underlying data should only have one owner
		 */
		explicit UniqueHandle(const UniqueHandle<T>& other) = delete;

		/**
		 * \brief Disallow Copy Assignment because of the ownership model
		 * \param other other handle
		 * \return none
		 */
		UniqueHandle<T>& operator=(const UniqueHandle<T>& other) = delete;

		/**
		 * \brief Create a new Handle by copying and invalidating the old one
		 * \param other The Handle that previously had the Data
		 */
		UniqueHandle(UniqueHandle<T, Deallocator>&& other) noexcept :
			data(nullptr),
			should_delete_deallocator(other.should_delete_deallocator),
			deallocator(other.deallocator) {
			
			// Transfer the Pointer since the old handle will not delete the underlying Data
			set_ptr(other.data);
			// Invalidate other Handle
			other.invalidate(false, false, false);
		}

		UniqueHandle<T, Deallocator>& operator=(UniqueHandle<T, Deallocator>&& other) noexcept {
			// We will invalidate this Handle
			invalidate();
			// Use other Deallocator
			deallocator = other.deallocator;
			should_delete_deallocator = other.should_delete_deallocator;
			// Copy old Data
			set_ptr(other.data);
			// Invalidate other Handle (but keeping the pointer alive)
			other.invalidate(false, false, false);
			// Return this Handle
			return *this;
		}

		/**
		 * \brief Destructor will free Data if the Handle is still valid
		 */
		~UniqueHandle() {
			if (is_valid()) {
				invalidate();
			}
		}

		/**
		 * \brief This function sets the data by copying the provided raw_data
		 * \param raw_data Copyable reference to already existing data
		 */
		void set(T&& raw_data) {
			if (data) {
				spdlog::warn("[UniqueHandle::set] Data in Handle has not been deleted before setting it");
				// Delete old data
				delete data;
			}
			data = new T(raw_data);
		}

		/**
		 * \brief Used to set the Data Pointer to a Heap-Allocated Data
		 * \param raw_ptr Pointer to the Data that should be controlled by the Handle
		 */
		void set_ptr(T* raw_ptr) {
			if (data) {
				spdlog::warn("[UniqueHandle::set_ptr] Data in Handle has not been deleted before setting it");
				// Delete old data
				delete data;
			}
			data = raw_ptr;
		}

		/**
		 * \brief used to check if this handle holds data
		 * \return true if data* differs from nullptr
		 */
		[[nodiscard]] bool is_valid() const {
			return data;
		}


		/**
		 * \brief This function must be called before new Data is assigned to the Handle
		 */
		void invalidate(const bool deallocate = true, const bool delete_data = true, const bool delete_allocator = true) {
			if constexpr (!std::is_same_v<Deallocator, NoDeallocator>) {
				if (data) {
					if (deallocate) deallocator->operator()(*data);
					if (delete_data) delete data;
				}
			}
			if (should_delete_deallocator && delete_allocator && deallocator) {
				delete deallocator;
				deallocator = nullptr;
			}
			data = nullptr;
		}

		/**
		 * \brief Conversion operator to the underlying data
		 */
		OVK_CONVERSION operator T() const {
			assert(data);
			return *data;
		}

		T* operator->() {
			return data;
		}

		const T* operator->() const {
			return data;
		}


#ifdef OVK_SPDLOG_COMPAT
		template<typename OStream>
		friend OStream& operator<<(OStream& os, const UniqueHandle<T, Deallocator>& c) {
			if (c.data == nullptr)
				return os << "Unique Handle [" << (c.is_valid() ? "valid" : "invalid") << "]: " << "empty";
			else
				return os << "Unique Handle [" << (c.is_valid() ? "valid" : "invalid") << "]: " << *c.data;
		}
#endif

		/**
		 * \brief Get a reference to the Object
		 * \return Fails if data is nullptr otherwise reference to object
		 */
		T& get() {
			assert(data);
			return *data;
		}

		/**
		 * \brief Get a const reference to the object
		 * \return Fails if data is nullptr
		 */
		const T& get() const {
			assert(data);
			return *data;
		}

		T* data;
		bool should_delete_deallocator;
		Deallocator* deallocator;
	};

	template <typename T>
	class DeviceObject {
	protected:
		friend Device;
	public:
		DeviceObject(const DeviceObject &other) = delete;
		DeviceObject(DeviceObject &&other) noexcept = default;
		DeviceObject & operator=(const DeviceObject &other) = delete;
		DeviceObject & operator=(DeviceObject &&other) noexcept = default;
	protected:
		explicit DeviceObject(vk::Device& d, T raw) : handle(std::move(raw), ObjectDestroy<T>(d)) {}
		explicit DeviceObject(vk::Device& d) : handle({}, ObjectDestroy<T>(d)) {}

		virtual ~DeviceObject() {}
	public:
		UniqueHandle<T> handle;
		OVK_CONVERSION operator T() const { return static_cast<T>(handle); }

	};

	template <typename T>
	std::unique_ptr<T> make_unique(T&& handle) {
		return std::make_unique<T>(std::move(handle));
	}

	template <typename T>
	std::shared_ptr<T> make_shared(T&& handle) {
		return std::make_shared<T>(std::move(handle));
	}
	
	template<>
	struct OVK_API ObjectDestroy<vk::Instance> {
		void operator()(vk::Instance& i) {
			i.destroy();
		}
	};

	template<>
	struct OVK_API ObjectDestroy<GLFWwindow*> {
		void operator()(GLFWwindow*& i) {
			glfwDestroyWindow(i);
			glfwTerminate();
		}
	};

	template<>
	struct OVK_API ObjectDestroy<vk::SurfaceKHR> {

		vk::Instance i;

		explicit ObjectDestroy(vk::Instance& _i) : i(_i) {};

		void operator()(vk::SurfaceKHR& s) const {
			i.destroySurfaceKHR(s);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::Device> {
		void operator()(vk::Device& s) const {
			s.destroy();
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::SwapchainKHR> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::SwapchainKHR& s) const {
			d.destroySwapchainKHR(s);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::ImageView> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::ImageView& iv) const {
			d.destroyImageView(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::RenderPass> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::RenderPass& iv) const {
			d.destroyRenderPass(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::DescriptorSetLayout> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::DescriptorSetLayout& iv) const {
			d.destroyDescriptorSetLayout(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::DescriptorPool> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::DescriptorPool& iv) const {
			d.destroyDescriptorPool(iv);
		}

	};
	
	template<>
	struct OVK_API ObjectDestroy<vk::PipelineLayout> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::PipelineLayout& iv) const {
			d.destroyPipelineLayout(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::Pipeline> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::Pipeline& iv) const {
			d.destroyPipeline(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::Framebuffer> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::Framebuffer& iv) const {
			d.destroyFramebuffer(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::Buffer> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::Buffer& iv) const {
			d.destroyBuffer(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::DeviceMemory> {

		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}

		void operator()(vk::DeviceMemory& iv) const {
			d.freeMemory(iv);
		}

	};

	template<>
	struct OVK_API ObjectDestroy<vk::CommandPool> {
		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}
		void operator()(vk::CommandPool& iv) const {
			d.destroyCommandPool(iv);
		}
	};

	template<>
	struct OVK_API ObjectDestroy<vk::Fence> {
		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}
		void operator()(vk::Fence& iv) const {
			d.destroyFence(iv);
		}
	};

	template<>
	struct OVK_API ObjectDestroy<vk::Semaphore> {
		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}
		void operator()(vk::Semaphore& iv) const {
			d.destroySemaphore(iv);
		}
	};

	template<>
	struct OVK_API ObjectDestroy<vk::Image> {
		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}
		void operator()(vk::Image& iv) const {
			d.destroyImage(iv);
		}
	};

	template<>
	struct OVK_API ObjectDestroy<vk::Sampler> {
		vk::Device d;
		explicit ObjectDestroy(vk::Device& _d) : d(_d) {}
		void operator()(vk::Sampler& iv) const {
			d.destroySampler(iv);
		}
	};
	
}



