#include "Security.hpp"

#include <cassert>
#include <cstdint>

namespace WinXP {

void CryptMessage(void* data, int len, uint32 key)
{
	assert(len % sizeof(uint32) == 0);

	const int dwordLen = len / 4;
	uint32* dwordPtr = reinterpret_cast<uint32*>(data);
	for (int i = 0; i < dwordLen; ++i)
		*dwordPtr++ ^= key;
}


uint32 GenerateChecksum(std::initializer_list<std::pair<const void*, size_t>> dataBuffers)
{
	uint32 checksum = htonl(0x12344321);
	for (const auto& dataBuffer : dataBuffers)
	{
		assert(dataBuffer.second % sizeof(uint32) == 0);

		const size_t dwordLen = dataBuffer.second / 4;
		const uint32* dwordPtr = reinterpret_cast<const uint32*>(dataBuffer.first);
		for (size_t i = 0; i < dwordLen; i++)
			checksum ^= *dwordPtr++;
	}
	return ntohl(checksum);
}

};
