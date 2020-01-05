#pragma once

#include "def.h"

namespace ovk {

#ifdef OVK_RENDERDOC_COMPAT

	template <typename T>
	struct TagInfo {
		static vk::DebugReportObjectTypeEXT get_type() { return vk::DebugReportObjectTypeEXT::eUnknown; }
		static uint64_t get_handle(T&) { return 0; }
	};
	
	template <typename T>
	struct Tagger {

		static void set_name(T&, const std::string &name, vk::Device device, PFN_vkDebugMarkerSetObjectNameEXT fn);

		template <typename Tag>
		static void set_tag(T&, Tag& data, vk::Device device, PFN_vkDebugMarkerSetObjectTagEXT fn);
		
	};

	// ************************************************************************************************************
	// Default Implementation of Tagger Functions
	
	template <typename T>
	void Tagger<T>::set_name(T & t, const std::string &name, vk::Device device, PFN_vkDebugMarkerSetObjectNameEXT fn) {
		const auto handle = TagInfo<T>::get_handle(t);
		const auto type = TagInfo<T>::get_type();
		if (!fn || type == vk::DebugReportObjectTypeEXT::eUnknown || handle == 0) {
			spdlog::error("Tagger<{}> attempted to name object that doesn't have a partial specialization on TagInfo!", typeid(T).name());
			assert(false);
		}
		VkDebugMarkerObjectNameInfoEXT name_info = {};
		name_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		// Type of the object to be named
		name_info.objectType = static_cast<VkDebugReportObjectTypeEXT>(type);
		// Handle of the object cast to unsigned 64-bit integer
		name_info.object = handle;
		// Name to be displayed in the offline debugging application
		name_info.pObjectName = name.c_str();
		fn(device, &name_info);
	}

	template <typename T>
	template <typename Tag>
	void Tagger<T>::set_tag(T & t, Tag &data, vk::Device device, PFN_vkDebugMarkerSetObjectTagEXT fn) {
		VkDebugMarkerObjectTagInfoEXT tag_info = {};
		tag_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
		tag_info.objectType = static_cast<VkDebugReportObjectTypeEXT>(TagInfo<T>::get_type());
		tag_info.object = TagInfo<T>::get_handle(t);
		tag_info.tagName = 0x01;
		// Size of the arbitrary data structure 
		tag_info.tagSize = sizeof(Tag);
		// Pointer to the arbitrary data
		tag_info.pTag = &data;
		fn(device, &tag_info);
	}

	// ************************************************************************************************************

#endif

	
}