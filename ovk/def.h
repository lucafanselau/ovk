#pragma once

#include "pch.h"

#include <spdlog/spdlog.h>
#include <fmt/ostream.h>

#include <rang.hpp>

#ifdef OVK_EXPORTS
#define OVK_API __declspec(dllexport)
#else
#define OVK_API __declspec(dllimport)
#endif

// Determine if a debug or release build is present
#ifdef NDEBUG
#define RELEASE
#else
#define DEBUG
#endif

// ImGui Utils (Like Allocator::draw_debug())
#define OVK_IMGUI_UTILS

// Define if you want to use SPDLOG Compatible Handles
#define OVK_SPDLOG_COMPAT
// Define if u want to use RenderDoc as an offline debugger with the debug marker extension
#ifdef DEBUG
#define  OVK_RENDERDOC_COMPAT
#endif

// Uncomment to use implicit conversion
#define OVK_USE_IMPLICIT_CONVERSION

#ifdef OVK_USE_IMPLICIT_CONVERSION
#define OVK_CONVERSION 
#else
#define OVK_CONVERSION explicit
#endif

#define panic(message, ...) spdlog::critical("[{}{}:{}{}] {}PANIC!{} {}", rang::fg::red, __FILE__, __LINE__, rang::style::reset, rang::bg::red, rang::style::reset, fmt::format(message, ##__VA_ARGS__))

#define ovk_asserts(expression, message, ...) if (!(expression)) { panic(message, ##__VA_ARGS__);}
#define ovk_assert(expression, ...) if (!(expression)) { panic("assertion failed!!");}

#define VK_ASSERT(result, message)						\
	if ((result) != vk::Result::eSuccess) { panic("Vulkan assertion failed! Message: {}", message); }


// Debug assert (Only check if in debug mode)
#if defined(DEBUG)
#define VK_DASSERT(result, message) VK_ASSERT(result, message)
#else
#define VK_DASSERT(result, message)
#endif

#define VK_CREATE(expression, message)							\
	[&]() {																						\
		auto [r, h] = expression;												\
		VK_ASSERT(r, message);													\
		return std::move(h);														\
	}()

#define VK_GET(expression)                \
	expression.value

#if defined(DEBUG)
#define VK_DCREATE(expression, message) VK_CREATE(expression, message)
#else
#define VK_DCREATE(expression, message) VK_GET(expression)
#endif

#define VK_STD_TIMEOUT std::numeric_limits<uint64_t>::max()

#define ovk_unique(T, expression) std::make_unique<T>(std::forward<T>(expression))
#define ovk_shared(T, expression) std::make_shared<T>(std::forward<T>(expression))



