#pragma once

#include "def.h"
#include "handle.h"
#include "surface.h"
#include "device.h"



namespace ovk {

#ifdef DEBUG
	// TODO: Hard code of validation layers
	const std::vector<const char*> validation_layers = {
		"VK_LAYER_KHRONOS_validation" };
#endif
	
	struct OVK_API AppInfo {
		std::string name;
		uint16_t version_major, version_minor, version_patch;
	};

	class OVK_API Instance {
	public:
#ifdef DEBUG
		Instance(AppInfo app_info, std::vector<std::string>&& additional_extensions, bool add_validation = true);
#else
		Instance(AppInfo app_info, std::vector<std::string>&& additional_extensions, bool add_validation = false);
#endif

		Instance(const Instance& o) = delete;
		Instance& operator=(const Instance& o) = delete;

		Surface create_surface(int width, int height, std::string title, bool init_events = false /* Flags?*/);

		Device create_device(std::vector<const char*>&& requested_extensions, vk::PhysicalDeviceFeatures features, Surface& s);
	private:
		// Unique Handle to 
		UniqueHandle<vk::Instance> instance;

#ifdef DEBUG
		struct OVK_API DebugUtils {
			~DebugUtils();
			vk::Instance* parent{};
			vk::DebugUtilsMessengerEXT messenger;
		} debug_utils;
#endif

	};


}
