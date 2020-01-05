#include "noise.h"

#include <imgui.h>
#include <noise/noise.h>

enum class NoiseType : uint32_t {

	perlin = 0,
	scale_bias = 1,
	add = 2,
	none = 3
};

const char* to_string(NoiseType e) {
	switch (e) {
	case NoiseType::perlin: return "perlin";
	case NoiseType::scale_bias: return "scale_bias";
	case NoiseType::add: return "add";
	case NoiseType::none: return "none";
	default: return "unknown";
	}
}

struct PerlinNoiseModule : NoiseModule {

	bool update(const char *label, NoiseBuilder& builder) override;
	noise::module::Module * get_module() override;

	noise::module::Perlin perlin;
};

struct ScaleBiasModule : NoiseModule {

	ScaleBiasModule(NoiseBuilder& builder);
	
	bool update(const char *label, NoiseBuilder& builder) override;
	noise::module::Module * get_module() override;

	noise::module::ScaleBias scale_bias;
	uint32_t active_module = 0;
};

struct AddModule : NoiseModule {
	explicit AddModule(NoiseBuilder& builder);

	bool combo(int number, uint32_t& active, const char *label, NoiseBuilder& builder);
	
	bool update(const char *label, NoiseBuilder &builder) override;
	noise::module::Module * get_module() override;

	noise::module::Add add;
	uint32_t active_1 = 0, active_2 = 0;
};

struct ExampleModule : NoiseModule {

	ExampleModule();

	noise::module::Perlin perlin, small;
	noise::module::ScaleBias scale, scale_small;
	noise::module::Add adder;
	
	bool update(const char *label, NoiseBuilder &builder) override;
	noise::module::Module * get_module() override;
};

NoiseBuilder::NoiseBuilder() {

	add_module<ExampleModule>("example");
	
}

bool NoiseBuilder::update() {

	bool changed = false;
	
	ImGui::Begin("Noise");
	if (ImGui::BeginCombo("Output Module", available_modules[output_module_index].c_str())) {
    for (auto i = 0; i < available_modules.size(); i++) {
    	auto &available = available_modules[i];
        bool is_selected = output_module_index == i;
        if (ImGui::Selectable(available_modules[i].c_str(), is_selected)) {
          output_module_index = i;
        	changed = true;
				}
        if (is_selected)
            ImGui::SetItemDefaultFocus(); 
    }
    ImGui::EndCombo();
	}
	

	ImGui::Text("Add Module:");
	
	if (ImGui::BeginCombo("Type", to_string(static_cast<NoiseType>(add_module_type)))) {
    for (auto i = 0; i < static_cast<uint32_t>(NoiseType::none); i++) {
	    const auto available = to_string(static_cast<NoiseType>(i));
      const bool is_selected = add_module_type == i;
      if (ImGui::Selectable(available, is_selected))
            add_module_type = i;
      if (is_selected)
            ImGui::SetItemDefaultFocus(); 
    }
    ImGui::EndCombo();
	}
	// ImGui::SameLine();
	ImGui::InputTextWithHint("Name", "Enter new Name!", add_module_name, 32);
	// ImGui::SameLine();
	if (ImGui::Button("Add!")) {
		switch (static_cast<NoiseType>(add_module_type)) {
		case NoiseType::perlin:
			add_module<PerlinNoiseModule>(std::string(add_module_name));
			break;
		case NoiseType::scale_bias:
			add_module<ScaleBiasModule>(std::string(add_module_name));
			break;
		case NoiseType::add:
			add_module<AddModule>(std::string(add_module_name));
			break;
		default:
			break;
		}
	}

	ImGui::Separator();

	// Modules
  ImGui::BeginGroup();
	
  ImGui::BeginChild(ImGui::GetID("Modules"), ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_None);

	int i = 0;
	for (auto& [name, noise_module] : modules) {
		
		auto label = fmt::format("Module #{}: {}", i, name);
		if (ImGui::CollapsingHeader(label.c_str())) {
			ImGui::BeginGroup();
			changed |= noise_module->update(label.c_str(), *this);
			ImGui::EndGroup();
		}

		i++;
	}

  ImGui::EndChild();
  ImGui::EndGroup();
	
	ImGui::End();

	
	return changed;
}

noise::module::Module * NoiseBuilder::get_module() {
	return modules[available_modules[output_module_index]]->get_module();
}

bool PerlinNoiseModule::update(const char *label, NoiseBuilder& builder) {
	bool changed = false;

	float frequency = perlin.GetFrequency();
	std::string gui_label = fmt::format("Frequency##{}", label);
	if (changed |= ImGui::SliderFloat(gui_label.c_str(), &frequency, 0.0f, 15.f); changed)
		perlin.SetFrequency(frequency);
		

	float lacunarity = perlin.GetLacunarity();
	gui_label = fmt::format("Lacunarity##{}", label);
	if (changed |= ImGui::SliderFloat(gui_label.c_str(), &lacunarity, 1.0f, 4.0f); changed)
		perlin.SetLacunarity(lacunarity);
		

	int octave_count = perlin.GetOctaveCount();
	gui_label = fmt::format("Octave Count##{}", label);
	if (changed |= ImGui::InputInt(gui_label.c_str(), &octave_count, 1, 1, 10); changed)
		perlin.SetOctaveCount(octave_count);
	
	float persistence = perlin.GetPersistence();
	gui_label = fmt::format("Persistence##{}", label);
	if (changed |= ImGui::SliderFloat(gui_label.c_str(), &persistence, 0.0f, 1.0f); changed)
		perlin.SetPersistence(persistence);


	return changed;
}

noise::module::Module * PerlinNoiseModule::get_module() {
	return &perlin;
}

ScaleBiasModule::ScaleBiasModule(NoiseBuilder& builder) {
	auto& new_module = builder.modules[builder.available_modules[active_module]];
	scale_bias.SetSourceModule(0, *new_module->get_module());
}

bool ScaleBiasModule::update(const char *label, NoiseBuilder& builder) {
	bool changed = false;

	float scale = scale_bias.GetScale();
	std::string gui_label = fmt::format("Scale##{}", label);
	if (changed |= ImGui::InputFloat(gui_label.c_str(), &scale, 0.1f, 1.0f); changed)
		scale_bias.SetScale(scale);

	float bias = scale_bias.GetBias();
	gui_label = fmt::format("Bias##{}", label);
	if (changed |= ImGui::SliderFloat(gui_label.c_str(), &bias, -5.0f, 5.0f); changed)
		scale_bias.SetBias(bias);

	gui_label = fmt::format("Active##{}", label);
	bool new_active = false;
	if (ImGui::BeginCombo(gui_label.c_str(), builder.available_modules[active_module].c_str())) {
    for (auto i = 0; i < builder.available_modules.size(); i++) {
      const bool is_selected = active_module == i;
      if (ImGui::Selectable(builder.available_modules[i].c_str(), is_selected)) {
        active_module = i;
      	new_active = true;
			}
      if (is_selected)
            ImGui::SetItemDefaultFocus(); 
    }
    ImGui::EndCombo();
	}
	if (new_active) {
		auto& new_module = builder.modules[builder.available_modules[active_module]];
		scale_bias.SetSourceModule(0, *new_module->get_module());
		changed = true;
	}
	
	return changed;
}

noise::module::Module * ScaleBiasModule::get_module() {
	return &scale_bias;
}

AddModule::AddModule(NoiseBuilder &builder) {
	auto& new_module1 = builder.modules[builder.available_modules[active_1]];
	add.SetSourceModule(0, *new_module1->get_module());
	auto& new_module2 = builder.modules[builder.available_modules[active_2]];
	add.SetSourceModule(1, *new_module2->get_module());

}

bool AddModule::combo(int number, uint32_t& active, const char *label, NoiseBuilder &builder) {
	auto changed = false;
	
	auto gui_label = fmt::format("Active {}##{}", number, label);
	bool new_active = false;
	if (ImGui::BeginCombo(gui_label.c_str(), builder.available_modules[active].c_str())) {
    for (auto i = 0; i < builder.available_modules.size(); i++) {
      const bool is_selected = active == i;
      if (ImGui::Selectable(builder.available_modules[i].c_str(), is_selected)) {
        active = i;
      	new_active = true;
			}
      if (is_selected)
            ImGui::SetItemDefaultFocus(); 
    }
    ImGui::EndCombo();
	}
	if (new_active) {
		auto& new_module = builder.modules[builder.available_modules[active]];
		add.SetSourceModule(number - 1, *new_module->get_module());
		changed = true;
	}

	return changed;
}



bool AddModule::update(const char *label, NoiseBuilder &builder) {

	auto changed = false;

	changed |= combo(1, active_1, label, builder);

	changed |= combo(2, active_2, label, builder);

	return changed;
}

noise::module::Module * AddModule::get_module() {
	return &add;
}

ExampleModule::ExampleModule() {

	perlin.SetFrequency(0.216f);
	perlin.SetLacunarity(1.67f);

	scale.SetScale(5.0f);
	scale.SetBias(2.5f);
	scale.SetSourceModule(0, perlin);

	small.SetFrequency(4.0f);
	small.SetLacunarity(2.5f);

	scale_small.SetScale(0.6f);
	scale_small.SetSourceModule(0, small);

	adder.SetSourceModule(0, scale_small);
	adder.SetSourceModule(1, scale);
}


bool ExampleModule::update(const char *label, NoiseBuilder &builder) {
	ImGui::Text("No configuration: its two scaled noises in an adder");
	return false;
}

noise::module::Module * ExampleModule::get_module() {
	return &adder;
}
