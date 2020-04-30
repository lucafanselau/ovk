#include "pch.h"
#include "instance.h"

#include "device.h"

#include <GLFW/glfw3.h>
#include <rang.hpp>
#include <spdlog/sinks/ansicolor_sink.h>

namespace ovk {

#ifdef DEBUG
	// Debug Messenger Callback
	VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {

		std::string message_type;
		switch (type) {
		case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
			message_type = "general";
			break;
		case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
			message_type = "performance";
			break;
		case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
			message_type = "validation";
			break;
		default:
			message_type = "unknown";
			break;
		}

		spdlog::level_t level;

		std::string message(data->pMessage);
		
		switch (severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			level = spdlog::level::debug;
			spdlog::debug("[{}]: {}", message_type, data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			// if (message_type == "general" || (message_type == "validation" && message.starts_with("OBJ"))) spdlog::trace("[{}]: {}", message_type, data->pMessage);
			/*else*/ spdlog::info("[{}]: {}", message_type, data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			spdlog::warn("[{}]: {}", message_type, data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			spdlog::error("[{}]: {}", message_type, data->pMessage);
			break;
		default:;
		}

		return VK_FALSE;
	}

#endif

	

	bool check_validation_layer_support() {
#ifdef DEBUG
		auto available_layers = vk::enumerateInstanceLayerProperties();

		for (auto&& requested : validation_layers) {
			auto found = false;
			for (auto&& available : available_layers.value) {
				if (!found && strcmp(requested, available.layerName) == 0) {
					found = true;
				}
			}
			if (!found) return false;
		}
		// All layers have been found
		return true;
#else
		return true;
#endif
	}

	std::vector<const char*> get_extensions(bool add_debug_extension) {
		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		std::vector<const char*> extensions(glfw_extensions,
			glfw_extensions + glfw_extension_count);
		if (add_debug_extension)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}

	Instance::Instance(AppInfo app_info, std::vector<std::string>&& additional_extensions, bool add_validation)
		: instance(vk::Instance(), {})
	{
		// TODO: Move that somewhere else but honestly its okay here
		// auto console = spdlog::default_factory::create<spdlog::sinks::ansicolor_stdout_sink_mt>("console");
		// spdlog::set_default_logger(console);
			
		spdlog::set_level(spdlog::level::trace);
		rang::setControlMode(rang::control::Force);
		rang::setWinTermMode(rang::winTerm::Ansi);

		
		if (!glfwInit())
			spdlog::error("failed to initialize glfw!");


		vk::ApplicationInfo vk_app_info(
			app_info.name.c_str(),
			VK_MAKE_VERSION(app_info.version_major, app_info.version_minor, app_info.version_patch),
			"ovk",
			VK_MAKE_VERSION(0, 0, 1),
			VK_API_VERSION_1_1
		);

		vk::InstanceCreateInfo create_info(
			{},
			&vk_app_info,
			0,
			nullptr,
			0,
			nullptr
		);

#ifdef DEBUG

		if (add_validation) {
			if (check_validation_layer_support()) {
				create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
				create_info.ppEnabledLayerNames = validation_layers.data();
			}
			else {
				spdlog::error("[ovk::Instance] Failed to find all requested validation layers");
			}
		}

#endif

		auto extensions = get_extensions(add_validation);

		for (auto&& additional : additional_extensions)
			extensions.push_back(additional.c_str());

		create_info.enabledExtensionCount = extensions.size();
		create_info.ppEnabledExtensionNames = extensions.data();

		instance.invalidate(false, false, false);

		instance.set(VK_CREATE(vk::createInstance(create_info), "failed to create Instance"));

#ifdef DEBUG
		{
			// Add Validation Layer Callback
			using severity_flags = vk::DebugUtilsMessageSeverityFlagBitsEXT;
			using message_types = vk::DebugUtilsMessageTypeFlagBitsEXT;
			const vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info(
				{},
				severity_flags::eError | severity_flags::eInfo | severity_flags::eVerbose | severity_flags::eWarning,
				message_types::eGeneral | message_types::ePerformance | message_types::eValidation,
				debug_callback,
				nullptr
			);

			vk::DispatchLoaderDynamic dldy;
			dldy.init(static_cast<VkInstance>(instance.get()), ::vkGetInstanceProcAddr);
			auto [messenger_result, handle] = instance.get().createDebugUtilsMessengerEXT(messenger_create_info, nullptr, dldy);
			if (messenger_result != vk::Result::eSuccess) spdlog::error("failed to create debug utils!");
			debug_utils.messenger = std::move(handle);
			debug_utils.parent = &instance.get();
		}
#endif

	}

	Surface Instance::create_surface(int width, int height, std::string title, bool init_events) {
		return Surface(width, height, title, init_events, &instance.get());
	}

	Device Instance::create_device(std::vector<const char *> &&requested_extensions, vk::PhysicalDeviceFeatures features, Surface &s) {
		return Device(std::move(requested_extensions), features, s, &instance.get());
	}

#ifdef DEBUG	
	Instance::DebugUtils::~DebugUtils() {
		vk::DispatchLoaderDynamic dldy;
		dldy.init(static_cast<VkInstance>(*parent), ::vkGetInstanceProcAddr);
		parent->destroyDebugUtilsMessengerEXT(messenger, nullptr, dldy);
	}
#endif
	
}
