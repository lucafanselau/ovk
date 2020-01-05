// pch.h: This is a precompiled header file.

#ifndef PCH_H
#define PCH_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL  0
#define VULKAN_HPP_TYPESAFE_CONVERSION 1
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#include <spdlog/spdlog.h>

#include <rang.hpp>

#endif //PCH_H
