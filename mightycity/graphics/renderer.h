#pragma once
#include "mesh.h"
#include <base/device.h>
#include "app/camera.h"

#include "ui/renderer.h"

namespace ovk {
	class SwapChain;
	class RenderCommand;
}

struct LightData {
	glm::vec3 light_dir;
	glm::vec3 view_pos;
};

struct LightUniform {
	glm::mat4 view;
	glm::mat4 projection;
};

struct Transform {
	glm::vec3 pos = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	float rotation;
};

struct Entity {
	Model* model;
	Transform transform;
};

struct PickInfo {
	Chunk* chunk;
	int local_x, local_z;
	uint8_t triangle_type;
	glm::vec3 triangle_base;
	glm::vec3 hit;
};

struct Chunk;
struct Terrain;

// Renderer should have the following outline
// struct {}Renderer {
//   MasterRenderer* (parent);
//
//	 void create_const_objects();
//	 void create_dynamic_objects();
//
//	 void recreate_swapchain();
//
//	 // Draw function for the type of mesh/model u want to support rendering
//	 void draw(_{more_explicit_description eg. (draw_water)})(Type* t);
//	 
//	 // either (will be called inline of a RenderPass that is managed by Master Renderer)
//	 void on_inline_render(int index, ovk::RenderCommand& cmd)
//	 // or (in an unmanaged state or maybe even a secondary command)
//	 void on_render(int index, ovk::RenderCommand& cmd)
//
//	 // and an update method (that might be on a different thread (MAAAYBE!))
//	 void on_update(float dt)
// };


// NonCopyable
struct MasterRenderer {

// Public API
public:
	MasterRenderer(std::shared_ptr<ovk::Device>& d, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Surface>& s);

	MasterRenderer(const MasterRenderer &other) = delete;
	MasterRenderer(MasterRenderer &&other) noexcept = default;
	MasterRenderer & operator=(const MasterRenderer &other) = delete;
	MasterRenderer & operator=(MasterRenderer &&other) noexcept = default;

	void post_init(ovk::Buffer* materials_buffer);
	
	bool update(float dt);
	void render();

	void draw_terrain(Chunk* mesh) const;
	void draw_mesh(Model* m, Transform transform) const;
	
	std::optional<PickInfo> pick(glm::vec2 mouse_pos, Terrain& terrain);
	
	uint32_t get_index();
	// U want to manipulate data that is not per frame (better wait than)
	void wait_last_frame();
	


private:

	friend struct TerrainRenderer;
	friend struct MeshRenderer;

	void create_const_objects();
	void create_dynamic_objects();
	void create_renderer();

	void create_objects();
	
	void recreate_swapchain();

	void build_command_buffer(uint32_t index, ovk::RenderCommand& cmd);

	// Render Context Objects
	std::shared_ptr<ovk::Device> device;
	std::shared_ptr<ovk::SwapChain> swapchain;
	std::shared_ptr<ovk::Surface> surface;

	// Renderers
	std::unique_ptr<ovk::ImGuiRenderer> imgui;
	std::unique_ptr<TerrainRenderer> terrain;
	std::unique_ptr<MeshRenderer> mesh;
	
	// Const objects
	uint32_t swapchain_index;
	std::unique_ptr<ovk::RenderPass> render_pass;
	struct {
		std::vector<ovk::Semaphore> image_available, render_finished;
		std::vector<ovk::Fence> in_flight_fences;
		uint32_t current_frame = 0;
	} sync;
	struct {
		std::vector<ovk::Image> depth_targets;
		std::vector<ovk::ImageView> depth_views;
		std::vector<ovk::Framebuffer> framebuffers;
		std::unique_ptr<ovk::RenderPass> renderpass;
		std::unique_ptr<ovk::DescriptorTemplate> descriptor_template;
		std::unique_ptr<ovk::DescriptorPool> descriptor_pool;
		std::vector<ovk::DescriptorSet> descriptor_sets;
		std::vector<ovk::Buffer> light_buffer;
	} shadow;

	// Scene Data (might be even a level higher (something like a Scene struct.)
	ovk::FirstPersonCamera camera;
	LightData lights;
	LightUniform light_uniform;

	std::vector<ovk::ImageView> rect_views;
	std::vector<ovk::ui::TexturedRect> rects;
	

	// Dynamic (Swapchain dependent objects)
	struct {
		std::vector<ovk::Framebuffer> swapchain_framebuffers;
		std::vector<std::unique_ptr<ovk::RenderCommand>> command_buffers;
		std::vector<ovk::Buffer> camera_uniform_buffers;
		std::vector<ovk::Buffer> light_uniform_buffers;
	} dynamic;
	struct {
		std::unique_ptr<ovk::Image> image;
		std::unique_ptr<ovk::ImageView> view;
	} depth;
	struct {
		std::vector<ovk::Image> color_targets;
		std::vector<ovk::ImageView> color_target_views;
		std::vector<ovk::Framebuffer> framebuffers;
		std::unique_ptr<ovk::Image> depth_image;
		std::unique_ptr<ovk::ImageView> depth_view;
		std::unique_ptr<ovk::RenderPass> render_pass;
		std::unique_ptr<ovk::Buffer> pick_buffer;
	} picker;
	
};

struct TerrainRenderer {

	TerrainRenderer(MasterRenderer* p);
	
	void create_const_objects();
	void create_dynamic_objects();
	
	void recreate_swapchain();
	
	void draw(Chunk* mesh);

	void on_shadow_render(int index, ovk::RenderCommand& cmd);
	
	void on_inline_render(int index, ovk::RenderCommand& cmd);

	// This is required to be called after layer render stage but before on_inline_render();
	void on_picker_render(int index, ovk::RenderCommand& cmd);
	
	// void on_update(float dt)

	MasterRenderer* parent;

	std::vector<Chunk*> jobs;
	std::unique_ptr<ovk::DescriptorTemplate> descriptor_template;

	// Dynamic
	struct {
		std::unique_ptr<ovk::GraphicsPipeline> pipeline, picker_pipeline;
		std::unique_ptr<ovk::DescriptorPool> descriptor_pool;
		std::vector<ovk::DescriptorSet> descriptor_sets;
	} dynamic;

	struct {

		std::unique_ptr<ovk::GraphicsPipeline> pipeline;

	} shadow;
	
};

struct WaterRenderer {

};

struct MeshRenderer {

public:
	MeshRenderer(MasterRenderer* p);

	void post_init(ovk::Buffer* materials_buffer);
	
	void draw(Model* m, Transform transform);
	// Is expected to be called inline with a render pass
	void on_shadow_render(int index, ovk::RenderCommand& cmd);
	
	void on_inline_render(int index, ovk::RenderCommand& cmd);

	void recreate();

private:

	void create_const_objects();
	void create_dynamic_objects();

	MasterRenderer *parent;
	
	std::vector<Entity> render_jobs;

	std::unique_ptr<ovk::DescriptorTemplate> descriptor_template;
	// std::unique_ptr<ovk::Buffer> material_buffer;
	ovk::Buffer* materials_buffer = nullptr;
	
	struct {
		std::unique_ptr<ovk::GraphicsPipeline> pipeline;
		std::unique_ptr<ovk::DescriptorPool> descriptor_pool;
		std::vector<ovk::DescriptorSet> descriptor_sets;
		// std::vector<ovk::Buffer> material_buffers;
	} dynamic;

	struct {
		std::unique_ptr<ovk::GraphicsPipeline> pipeline;
	} shadow;

};
