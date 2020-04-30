#pragma once

#include "../model_loader.h"

namespace ovk::util {
	
	Model impl_load_model_obj(const std::string& filepath, ParseOptions options, ovk::Device& device);

}
