#include "signature_scan.h"
#include <cstdio>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #value); return 1; } } while (0)

int main()
{
	const uint8_t code[] = {0x55, 0x48, 0x89, 0xE5, 0x90, 0x55, 0x48, 0x89, 0xE5};
	const uint8_t* match = nullptr;
	size_t count = 0;
	ScanSignature(code, sizeof(code), ParseSignature("55 48 ? E5"), match, count);
	CHECK(count == 2); // Ambiguous signatures must never be used as a function.
	count = 0;
	ScanSignature(code, sizeof(code), ParseSignature("90 55 ?? 89 E5"), match, count);
	CHECK(count == 1 && match == code + 4); // Last possible match is included.
	count = 0;
	ScanSignature(code, 2, ParseSignature("55 48 89 E5"), match, count);
	CHECK(count == 0);
	CHECK(ParseSignature("55 GG").empty());
	CHECK(ParseSignature("55 4").empty());
	CHECK(ParseSignature("").empty());
	return 0;
}
