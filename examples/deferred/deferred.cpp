#include <base/device.h>
#include <base/instance.h>
#include <spdlog/spdlog.h>

#include <util/model_loader.h>

#include "renderer.h"

class DeferredExample {
public:
  ovk::Instance instance;
  ovk::Surface surface;
  ovk::Device device;

  std::unique_ptr<ovk::util::Model> model;
  std::unique_ptr<Renderer> renderer;

  DeferredExample();
  void run();

  void create_model();
};

std::vector<const char *> get_required_extensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

vk::PhysicalDeviceFeatures get_features() {
  return vk::PhysicalDeviceFeatures();
}

DeferredExample::DeferredExample()
    : instance(ovk::AppInfo{"Deferred", 0, 0, 1}, {}),
      surface(std::move(
          instance.create_surface(1960, 1080, "Deferred Shading", true))),
      device(std::move(instance.create_device(get_required_extensions(),
																						 get_features(), surface))) {

  // Now that we have that the rendering context of the application is already
  // finished

  spdlog::info("Starting Example: Deferred Shading");

	create_model();
	
	// Create the Renderer
	renderer = std::make_unique<Renderer>(&device, &surface, model.get());
	
	
}

void DeferredExample::run() {

	spdlog::info("start main loop");
	while(surface.update()) {

		// Figure out delta time
		static int fps = 0;
		float delta;
		{
		using namespace std::chrono;

			static auto timer = 0.0f;
			static int frames = 0;

			static auto startup = high_resolution_clock::now();

			auto now = high_resolution_clock::now();
			auto time = duration<float, seconds::period>(now - startup).count();
			static auto last_time = time;

			delta = time - last_time;
			last_time = time;

			frames++;
			
			timer += delta;

			if (timer >= 1.0f) {
				timer -= 1.0f;
				fps = frames;
				frames = 0;
			}
			
		}
		
		renderer->update(delta);

		// Maybe we want to do something

		renderer->render();
	}

	renderer->finish();
	
}

void DeferredExample::create_model() {
  ovk::util::ParseOptions parse_options{
      .disable_callback = false,
      .callback = [](const ovk::util::VertexData &data,
                     ovk::util::OutputBuffer &output) {
        // We want to use the Render Vertex Layout
        RenderVertex vertex{.pos = data.pos, .normal = data.normal};
        output.push_data(&vertex, sizeof(vertex));
      }};

  model = ovk::make_unique(
      ovk::util::load_model("res/models/pug.obj", parse_options, device));
}

int main(int argc, char **argv) {
  try {
    DeferredExample example;
    example.run();
  } catch (const std::exception &e) {
    // This should not happen but maybe it will and we dont want everything to
    // crash
    spdlog::critical("Exception Thrown: ");
    spdlog::critical(e.what());
  }
};
