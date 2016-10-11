#include "stdafx.h"
#include "CryptoSaver.h"


#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/endian/arithmetic.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp> 

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <boost/algorithm/string.hpp>

#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/evp.h>


typedef boost::endian::big_uint64_at uint64be_t;

typedef
BOOLEAN(*RtlGenRandom_t)(
	_Out_ PVOID RandomBuffer,
	_In_  ULONG RandomBufferLength
	);

class WinRandomGen : public RandomGen
{
public:
	WinRandomGen()
	{
		hLib = LoadLibrary(L"advapi32.dll");
		if (!hLib)
			throw std::runtime_error("Could not load advapi32.dll");

		gen_rand = (RtlGenRandom_t)GetProcAddress(hLib, "SystemFunction036");
		if (!gen_rand)
			throw std::runtime_error("Could not load RtlGenRandom");

	}

	virtual ~WinRandomGen()
	{
		FreeLibrary(hLib);
	}

	virtual void generate(uint8_t * buffer, size_t& len)
	{
		gen_rand(buffer, (ULONG)len);
	}
private:
	HMODULE hLib;
	RtlGenRandom_t gen_rand;
};



RandomGen::Ptr RandomGen::create()
{
	return boost::make_shared<WinRandomGen>();
}


CryptoKey::CryptoKey()
{

}

CryptoKey::~CryptoKey()
{

}

const uint8_t * CryptoKey::get()
{
	return m_public_key.get();
}

size_t CryptoKey::size()
{
	return m_public_key_size;
}

CryptoKey CryptoKey::load(std::string const& filename)
{
	namespace bs = boost::spirit;
	namespace qi = boost::spirit::qi;
	namespace phx = boost::phoenix;
	using namespace bs;
	using namespace qi::standard;


	std::string contents;
	fs::ifstream in(filename, std::ios::in | std::ios::binary);
	if (!in)
		throw std::runtime_error((std::string("Cannot open file ") + filename).c_str());

	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();

	auto first = std::begin(contents);
	auto last = std::end(contents);

	std::string key;

	qi::rule<std::string::const_iterator, std::string()> key_rule =
		lit("-----BEGIN RSA PUBLIC KEY-----\n")
			> lexeme[*(char_("A-Za-z0-9/+=\n"))]
			> "-----END RSA PUBLIC KEY-----\n";

	bool r = qi::phrase_parse(first, last, key_rule[phx::ref(key) = bs::_1], blank);
	if (!r )
	{
		std::cerr << L"Failed to read public key file" << std::endl;
		throw std::runtime_error("Failed to read public key file");
	}

	std::vector<uint8_t> result;

	{
		using namespace boost::archive::iterators;
		typedef transform_width< binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6 > it_binary_t;

		std::string contents = b::trim_copy(key);
		size_t paddChars = std::count(contents.begin(), contents.end(), '=');
		result.assign(it_binary_t(contents.begin()), it_binary_t(contents.end())); // decode
		result.erase(result.end() - paddChars, result.end());  // erase padding '\0' characters
	}

	CryptoKey cryptokey;

	cryptokey.m_public_key_size = result.size();
	cryptokey.m_public_key = b::make_shared<uint8_t[] >(cryptokey.m_public_key_size);
	std::copy(std::begin(result), std::end(result), cryptokey.m_public_key.get());

	return cryptokey;
}



CryptoSaver::CryptoSaver(const CryptoKey& key, RandomGen * pRandom)
	: m_rsa(nullptr)
	, m_key(key)
	, m_pRandom(pRandom)
{
	m_rsa = RSA_new();
	const uint8_t * p_key = m_key.get();
	d2i_RSAPublicKey(&m_rsa, &p_key, (long)m_key.size());

}

CryptoSaver::~CryptoSaver()
{
	RSA_free(m_rsa);
}

bool CryptoSaver::save(const fs::path& filename, uint8_t const* txt, size_t txt_size)
{
	if (!m_rsa)
	{
		std::vector<char> err(1024);
		ERR_error_string_n(ERR_get_error(), err.data(), err.size());
		std::cerr << L"Internal activation error: " << err.data() << std::endl;
		return false;
	}

	{
		uint8_t buffer[32];
		size_t len = sizeof(buffer);
		m_pRandom->generate(buffer, len);
		RAND_seed(buffer, (int)len);
		memset(buffer, 0, sizeof(buffer));
	}

	const size_t key_size = RSA_size(m_rsa);

	std::vector<uint8_t> aesPassword(key_size - 42, 0); //The Ultimate Question of Life, the Universe, and Everything

	{
		size_t sz = aesPassword.size();
		m_pRandom->generate(aesPassword.data(), sz);
		if(sz < aesPassword.size())
			aesPassword.erase(aesPassword.end() - sz, aesPassword.end());
	}

	std::vector<uint8_t> encryptedPassword;

	{
		encryptedPassword.resize((aesPassword.size() / key_size + 1)*key_size + 10);

		{
			int step = RSA_public_encrypt((int)aesPassword.size(),
				aesPassword.data(),
				encryptedPassword.data(),
				m_rsa,
				RSA_PKCS1_OAEP_PADDING);
			if (step < 0)
			{
				std::vector<char> err(1024);
				ERR_error_string_n(ERR_get_error(), err.data(), err.size());
				std::cerr << L"Failed to store activation data: " << err.data() << std::endl;
				return false;
			}

			if (step < encryptedPassword.size())
				encryptedPassword.erase(encryptedPassword.end() - (encryptedPassword.size() - step), encryptedPassword.end());
		}
	}

	std::vector<uint8_t> ciphered_data;

	{
		EVP_CIPHER_CTX *ctx;
		const EVP_CIPHER *cipher;
		const EVP_MD *dgst = NULL;
		const uint8_t *salt = NULL;
		uint8_t key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
		std::vector<uint8_t> block;
		int len = 0, written = 0;

		block.resize(txt_size + sizeof(key) + 1);

		if (!(ctx = EVP_CIPHER_CTX_new()))
		{
			std::cerr << ("Failed to initialize cipher") << std::endl;
			return false;
		}

		cipher = EVP_get_cipherbyname("aes-256-cbc");
		if (!cipher)
		{
			std::cerr << ("EVP_get_cipherbyname failed") << std::endl;
			return false;
		}

		dgst = EVP_get_digestbyname("md5");
		if (!dgst)
		{
			std::cerr << ("EVP_get_digestbyname failed") << std::endl;
			return false;
		}

		if (!EVP_BytesToKey(cipher, dgst, salt, aesPassword.data(), (int)aesPassword.size(), 1, key, iv))
		{
			std::cerr << ("EVP_BytesToKey failed") << std::endl;
			return false;
		}

		if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
		{
			std::cerr << ("Failed to initialize AES") << std::endl;
			return false;
		}

		if (1 != EVP_EncryptUpdate(ctx, block.data(), &len, txt, (int)txt_size))
		{
			std::cerr << ("EVP_EncryptUpdate failed") << std::endl;
			return false;
		}

		written += len;

		if (1 != EVP_EncryptFinal_ex(ctx, block.data() + written, &len))
		{
			std::cerr << ("EVP_EncryptFinal_ex failed") << std::endl;
			return false;
		}

		written += len;

		ciphered_data.insert(std::end(ciphered_data), block.begin(), block.begin() + written);

		EVP_CIPHER_CTX_free(ctx);
	}


	//write data to file
	{
		uint64be_t dataSize = (uint64_t)encryptedPassword.size();
		fs::ofstream os(filename, std::ios::out | std::ios::binary);
		if (!os)
		{
			std::cerr << ("Failed to open file: ") << filename << std::endl;
			return false;
		}
		os.write((const char*)&dataSize, sizeof(dataSize));
		os.write((const char*)encryptedPassword.data(), encryptedPassword.size());

		dataSize = ciphered_data.size();
		os.write((const char*)&dataSize, sizeof(dataSize));
		os.write((const char*)ciphered_data.data(), ciphered_data.size());
	}

	return true;
}


