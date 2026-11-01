#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

inline std::vector<int> ParseSignature(const char* text)
{
	std::vector<int> bytes;
	auto hex = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
	};
	while (*text)
	{
		if (*text == ' ') { ++text; continue; }
		if (*text == '?')
		{
			bytes.push_back(-1);
			++text;
			if (*text == '?') ++text;
		}
		else
		{
			const int high = hex(*text++);
			if (!*text) return {};
			const int low = hex(*text++);
			if (high < 0 || low < 0) return {};
			bytes.push_back(high * 16 + low);
		}
		if (*text && *text != ' ') return {};
	}
	return bytes;
}

inline void ScanSignature(const uint8_t* memory, size_t size, const std::vector<int>& pattern,
	const uint8_t*& match, size_t& count)
{
	if (pattern.empty() || size < pattern.size()) return;
	for (size_t i = 0; i <= size - pattern.size(); ++i)
	{
		size_t j = 0;
		for (; j < pattern.size(); ++j)
			if (pattern[j] >= 0 && memory[i + j] != pattern[j]) break;
		if (j == pattern.size()) { match = memory + i; ++count; }
	}
}
