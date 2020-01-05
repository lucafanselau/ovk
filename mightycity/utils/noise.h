#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <noise/module/modulebase.h>
#include "def.h"

struct NoiseBuilder;

struct NoiseModule {
	virtual ~NoiseModule() = default;
	virtual bool update(const char *label, NoiseBuilder& builder) = 0;
	virtual noise::module::Module* get_module() = 0;
};


struct NoiseBuilder {

	NoiseBuilder();

	bool update();

	template <typename T>
	void add_module(std::string name);

	noise::module::Module* get_module();
	
	std::vector<std::string> available_modules;
	size_t output_module_index = 0;
	std::unordered_map<std::string, std::unique_ptr<NoiseModule>> modules;

	uint32_t add_module_type = 0;
	char add_module_name[32] = "";
};



template <typename T>
void NoiseBuilder::add_module(std::string name) {
	static_assert(std::is_base_of_v<NoiseModule, T>, "[add_module] T must be a derived class from NoiseModule");
	static_assert(std::is_default_constructible_v<T> || std::is_constructible_v<T, NoiseBuilder&>, "[add_module] T must be default constructible or with (NoiseBuilder&)!");

	if (std::find(available_modules.begin(), available_modules.end(), name)  != available_modules.end()) {
		spdlog::info("Name already in use");
		return;
	}
	available_modules.push_back(name.c_str());
	if constexpr (std::is_default_constructible<T>::value) {
		auto [it, insertion] = modules.insert_or_assign(name, std::make_unique<T>());
		ovk_asserts(insertion, "Name can not be given twice");
	} else {
		auto [it, insertion] = modules.insert_or_assign(name, std::make_unique<T>(*this));
		ovk_asserts(insertion, "Name can not be given twice");
	}
	
}
