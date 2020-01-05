#pragma once
#include "handle.h"

struct GLFWwindow;

namespace ovk {
	
	// Target that can be addressed for rendering (typically a window)
	class OVK_API Surface {
		
	public:

		bool update();

		void wait_minimal();
		
	protected:
		Surface(int width, int height, std::string title, bool init_events, vk::Instance* parent);
		friend class Instance;
		friend class Device;
	public:
		UniqueHandle<GLFWwindow*> window;
		UniqueHandle<vk::SurfaceKHR> surface;
	};

}
