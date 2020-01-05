#pragma once

// This means to be a (very, very) basic implementation of a renderer
// This  will not implement a certain rendering technique or vertex layout
// this will just take a rendering context (instance?? (maybe), logical device, a given swapchain)
// there will be some functions that must be overloaded

#include "def.h"

namespace ovk {
	class Device;
	class SwapChain;
	class Surface;
}


namespace ovk::renderer {

	struct OVK_API Base {
  public:

	};

}
