#include "pch.h"
#include "manager.h"

namespace ovk::ui {

	Manager::Manager(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d)
	  : renderer(rp, sc, d) {

	}

	void Manager::text_panel(uint32_t x, uint32_t y, std::string text) {

		renderer.draw_text(x, y, text);
		
	}

	void Manager::on_draw(ovk::RenderCommand& cmd, uint32_t index) {
		renderer.on_inline_render(cmd, index);
	}

	
}
