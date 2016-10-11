#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <boost/program_options.hpp>
#include <boost/locale.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/endian/arithmetic.hpp>


#include <string>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/rand.h>

namespace b = ::boost;
namespace fs = b::filesystem;
namespace po = b::program_options;

typedef boost::endian::big_uint64_at uint64be_t;

class Decoder
{
public:
	Decoder();
	~Decoder();
	void work();
	bool init(int argc, const char *argv[]);

private:
	void initOpenSSL();
	void freeOpenSLL();
	void loadRSA();
	void decodeFile(fs::path const& p);

	std::string m_keyname;
	RSA * m_rsa;
};


Decoder::Decoder()
	: m_rsa(nullptr)
{
	initOpenSSL();
}

Decoder::~Decoder()
{
	if(m_rsa)
		RSA_free(m_rsa);

	freeOpenSLL();
}

bool Decoder::init(int argc, const char *argv[])
{
	po::options_description desc;
	std::string cfg_filename;
	std::string startDate;
	std::string endDate;


	po::options_description general_desc("General options");
	general_desc.add_options()
		("help,h", "Show help")
		("cfgfile,c", po::value<std::string>(&cfg_filename), "Cfg file name")
		("key,k", po::value<std::string>(&m_keyname), "Key")
		;

	desc.add(general_desc);

	po::variables_map vm;
	po::parsed_options parsedline = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
	po::store(parsedline, vm);
	po::notify(vm);
	if (vm.count("help"))
	{
		std::cout << desc << std::endl;
		return false;
	}

	if (!cfg_filename.empty())
	{
		fs::ifstream file(cfg_filename);
		if (file)
		{
			po::store(po::parse_config_file(file, desc), vm);
		}
		else
		{
			std::cout << L"Could not load configuration file " << cfg_filename << L". Exiting." << std::endl;
			return false;
		}
	}
	else
	{
		fs::ifstream file("decoder.cfg");
		po::store(po::parse_config_file(file, desc), vm);
	}
	po::store(parsedline, vm); //overwrite data from file
	po::notify(vm);

	if (m_keyname.empty())
	{
		std::cout << desc << std::endl;
		return false;
	}

	return true;
}


void Decoder::initOpenSSL()
{
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	OPENSSL_config(NULL);
}

void Decoder::freeOpenSLL()
{
	EVP_cleanup();
}

void Decoder::loadRSA()
{
	FILE *keyfile = fopen(m_keyname.c_str(), "r");
	if (!keyfile)
		throw std::runtime_error("There is no key file");
	m_rsa = PEM_read_RSAPrivateKey(keyfile, NULL, NULL, NULL);
	fclose(keyfile);
}


void Decoder::decodeFile(fs::path const& p)
{
	std::vector<uint8_t> contents;

	size_t pass_size = 0;
	size_t pass_offset = 0;

	size_t data_size = 0;
	size_t data_offset = 0;

	{
		fs::ifstream in(p, std::ios::in | std::ios::binary);
		if (!in)
			throw std::runtime_error((std::string("Cannot open file ") + p.string()).c_str());

		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read((char*)contents.data(), contents.size());
		in.close();
	}

	{
		uint64be_t size;
		uint8_t * ptr = contents.data();

		size = *reinterpret_cast<uint64be_t*>(ptr);
		ptr += sizeof(uint64be_t);

		pass_size = size;
		pass_offset = ptr - contents.data();

		ptr += size;

		if(pass_size + pass_offset > contents.size() - sizeof(uint64be_t))
			throw std::runtime_error((std::string("Invalid file ") + p.string()).c_str());

		size = *reinterpret_cast<uint64be_t*>(ptr);
		ptr += sizeof(uint64be_t);

		data_size = size;
		data_offset = ptr - contents.data();

		if (data_size + data_offset > contents.size())
			throw std::runtime_error((std::string("Invalid file ") + p.string()).c_str());

	}

	std::vector<uint8_t> password;
	{
		password.resize(pass_size);

		int step = RSA_private_decrypt((int)pass_size,
													contents.data() + pass_offset,
													password.data(),
													m_rsa,
													RSA_PKCS1_OAEP_PADDING);

		if (step < 0)
			throw std::runtime_error("There is decoding error");

		password.erase(password.begin() + step, password.end());
	}

	std::vector<uint8_t> unciphered_data;

	{
		EVP_CIPHER_CTX *ctx;
		const EVP_CIPHER *cipher;
		const EVP_MD *dgst = NULL;
		const uint8_t *salt = NULL;
		uint8_t key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
		std::vector<uint8_t> block;
		int len = 0, written = 0;

		block.resize(data_size + sizeof(key) + 1);

		if (!(ctx = EVP_CIPHER_CTX_new()))
			throw std::runtime_error("EVP_CIPHER_CTX_new");

		cipher = EVP_get_cipherbyname("aes-256-cbc");
		if (!cipher)
			throw std::runtime_error("EVP_get_cipherbyname");

		dgst = EVP_get_digestbyname("md5");
		if (!dgst)
			throw std::runtime_error("EVP_get_digestbyname");

		if (!EVP_BytesToKey(cipher, dgst, salt, password.data(), (int)password.size(), 1, key, iv))
			throw std::runtime_error("EVP_BytesToKey");

		if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
			throw std::runtime_error("EVP_DecryptInit_ex");

		if (1 != EVP_DecryptUpdate(ctx, block.data(), &len, contents.data() + data_offset, (int)data_size))
			throw std::runtime_error("EVP_DecryptUpdate");

		written += len;

		if (1 != EVP_DecryptFinal_ex(ctx, block.data() + written, &len))
			throw std::runtime_error("EVP_DecryptFinal_ex");

		written += len;

		unciphered_data.insert(std::end(unciphered_data), block.begin(), block.begin() + written);

		EVP_CIPHER_CTX_free(ctx);
	}

	{
		auto parent = p.parent_path();
		auto stem = p.stem().string();
		auto filename = parent / stem;

		fs::ofstream os(filename, std::ios::out | std::ios::binary);
		if (!os)
		{
			throw std::runtime_error("Failed to create file");
		}
		os.write((const char*)unciphered_data.data(), unciphered_data.size());

	}
}


void Decoder::work()
{
	loadRSA();

	for (fs::recursive_directory_iterator it{ fs::current_path() };it != fs::recursive_directory_iterator{}; it++)
	{
		if (fs::is_regular_file(*it) && it->path().extension() == ".crypto")
		{
			decodeFile(*it);
		}
	}
}




int main(int argc, const char *argv[])
{
	try
	{
		Decoder d;
		if(!d.init(argc, argv))
			return -1;
		d.work();
	}
	catch (std::exception& )
	{
		
	}
	catch (boost::exception& )
	{
	}

  return 0;
}

