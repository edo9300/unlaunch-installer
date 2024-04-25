#ifndef SHA1DIGEST_H
#define SHA1DIGEST_H

#include <array>
#include <string_view>

static constexpr int SHA1_LEN = 20;

class Sha1Digest {
public:
	friend constexpr inline bool operator==(const Sha1Digest& lhs, const Sha1Digest& rhs);
	Sha1Digest() {}
	constexpr Sha1Digest(std::string_view digestString)
	{
        auto hex2int = [](char ch) {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            return -1;
        };
		for(size_t i = 0; i < SHA1_LEN; ++i) {
			auto c1 = digestString[i * 2];
			auto c2 = digestString[(i * 2) + 1];
            digest[i] = hex2int(c1) << 4 | hex2int(c2);
		}
	}
	
	auto data()
	{
		return digest.data();
	}
	
private:	
	std::array<uint8_t, SHA1_LEN> digest{};
	
};

constexpr inline bool operator==(const Sha1Digest& lhs, const Sha1Digest& rhs)
{
    return lhs.digest == rhs.digest;
}

consteval Sha1Digest operator ""_sha1(const char* args, size_t len) {
	return Sha1Digest{std::string_view{args, len}};
}


#endif