#pragma once

struct rsa_st;
typedef struct rsa_st RSA;

class RandomGen
{
public:
	typedef boost::shared_ptr<RandomGen> Ptr;

	virtual ~RandomGen() {}
	virtual void generate(uint8_t * buffer, size_t& len) = 0;

	static RandomGen::Ptr create();
};

class CryptoKey
{
public:
	CryptoKey();
	~CryptoKey();

	const uint8_t * get();
	size_t size();

	static CryptoKey load(std::string const& filename);
private:
	b::shared_ptr<uint8_t[]> m_public_key;
	size_t m_public_key_size;
};

class CryptoSaver
{
public:
	CryptoSaver(const CryptoKey& pKey, RandomGen * pRandom);
	~CryptoSaver();

	bool save(const fs::path& filename, uint8_t const* txt, size_t txt_size);
private:

	RSA * m_rsa;
	CryptoKey m_key;
	RandomGen * m_pRandom;
};
