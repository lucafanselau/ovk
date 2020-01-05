// ovk.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "ovk.h"

void test() {
	std::string s = "Hello";
	s += "A";
	spdlog::info(s);
	return;
}
