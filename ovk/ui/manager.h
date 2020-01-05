#pragma once

#include "handle.h"

#include "renderer.h"

namespace ovk::ui {

	class OVK_API Manager {
  public:
		Manager(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d);

		void text_panel(uint32_t x, uint32_t y, std::string text);

		void on_draw(ovk::RenderCommand& cmd, uint32_t index);
		
	private:


		
		Renderer renderer;
    												 
	};
	
}
