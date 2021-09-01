#pragma once

class cudaCaller
{
public:
	static bool canCuda();
	static bool doCuda(std::vector<unsigned char>& hostBits, std::vector<unsigned char>& hostBytes, int sizeBits);
};
