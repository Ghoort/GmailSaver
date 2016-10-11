#include "stdafx.h"
#include "Jobs.h"

#include "TransactionManager.h"
#include "Downloader.h"
#include "GoogleTokens.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/karma.hpp>

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_uint.hpp>
#include <boost/spirit/include/qi_lit.hpp>

#include <ciere/json/value.hpp>




namespace karma = b::spirit::karma;

const std::string crypt_ext = ".crypto";


Job::Job(bio::io_service& service, Downloader& downloader, TransactionManager& manager)
	: m_service(service)
	, m_downloader(downloader)
	, m_manager(manager)
{

}

Job::~Job()
{

}

void Job::finilize(bool res)
{
	m_downloader.finilizeJob(res);
}



ListJob::ListJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager)
	: Job(service, downloader, manager)
{
	m_startDate = 0;
	m_endDate = 0;
}

bool ListJob::process(const std::string& token)
{
	std::stringstream sz_str;
	sz_str << karma::format(karma::lit("/gmail/v1/users/me/messages?") 
		<< "q=after%3A" << karma::int_ << "+before%3A" << karma::int_
		<< "&key=" << karma::string 
		<< ((karma::eps(!m_nextPage.empty()) << "&pageToken=" << karma::string ) | "" )
		<< "&access_token=" << karma::string,
		m_startDate, m_endDate, api_key, m_nextPage, token);


	auto pTrans = m_manager.create();
	pTrans->request().verb = "GET";
	pTrans->request().path = sz_str.str();
	pTrans->request().headers["Host"] = "www.googleapis.com";

	pTrans->send(boost::bind(&ListJob::onProcess, this, ::_1));
	return true;
}

void ListJob::init(time_t startDate, time_t endDate, std::string const& nextPage)
{
	m_nextPage = nextPage;
	m_startDate = startDate;
	m_endDate = endDate;
}


void ListJob::onProcess(b::shared_ptr<HttpTransaction> pTrans)
{
	if (pTrans->response().code >= 400)
	{
		std::cerr << "Error code on downloading" << std::endl;
		finilize(false);
		return;
	}

	ciere::json::value json_val;
	bool res = pTrans->response().getJson(json_val);

	if (!res)
	{
		std::cerr << "Error json processing in downloading" << std::endl;
		finilize(false);
		return;
	}

	if (!json_val.has_key("messages"))
	{
		//there is no msgs at this time interval;
		finilize(true);
		return;
	}

	std::vector<b::shared_ptr<Job> > nextJobs;

	if (json_val.has_key("nextPageToken"))
	{
		std::string const& nextPage = json_val["nextPageToken"];
		auto p = b::make_shared<ListJob>(m_service, m_downloader, m_manager);
		p->init(m_startDate, m_endDate, nextPage);
		nextJobs.push_back(p);
	}


	const auto& arr = json_val["messages"];
	for (auto it = arr.begin_array(); it != arr.end_array(); ++it)
	{
		const auto& id = (*it)["id"];

		auto p = b::make_shared<MsgJob>(m_service, m_downloader, m_manager);
		p->init(id);
		nextJobs.push_back(p);
	}

	m_downloader.createJobs(nextJobs);

	finilize(true);
}



MsgJob::MsgJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager)
	: Job(service, downloader, manager)
{

}

bool MsgJob::process(const std::string& token)
{
	std::stringstream sz_str;
	sz_str << karma::format(karma::lit("/gmail/v1/users/me/messages/") << karma::string 
		<< "?format=full"
		<< "&key=" << karma::string
		<< "&access_token="	<< karma::string,
		m_id, api_key, token);


	auto pTrans = m_manager.create();
	pTrans->request().verb = "GET";
	pTrans->request().path = sz_str.str();
	pTrans->request().headers["Host"] = "www.googleapis.com";

	pTrans->send(boost::bind(&MsgJob::onProcess, this, ::_1));

	return true;
}

void MsgJob::init(const std::string& id)
{
	m_id = id;
}

void MsgJob::onProcess(b::shared_ptr<HttpTransaction> pTrans)
{
	if (pTrans->response().code >= 400)
	{
		std::cerr << "Error code on downloading" << std::endl;
		finilize(false);
		return;
	}

	ciere::json::value msg;
	bool res = pTrans->response().getJson(msg);

	if (!res)
	{
		std::cerr << "Error json processing in downloading" << std::endl;
		finilize(false);
		return;
	}

	//read attachments

	std::vector<b::shared_ptr<Job> > nextJobs;

	auto& payload = msg["payload"];
	if (payload.has_key("parts"))
	{
		auto& payloadParts = payload["parts"];
		for (auto it = payloadParts.begin_array(); it != payloadParts.end_array(); ++it)
		{
			auto& body = (*it)["body"];
			if (body.has_key("attachmentId"))
			{
				auto p = b::make_shared<AttachJob>(m_service, m_downloader, m_manager);
				p->init(body["attachmentId"], m_id, (*it)["filename"], body["size"].get<int64_t>());
				nextJobs.push_back(p);
			}
		}
	}
	m_downloader.createJobs(nextJobs);

	//store msg data here

	{
		CryptoSaver saver(m_downloader.getKey(), m_downloader.getRandom().get());
		b::system::error_code ec;
		fs::path p("content");
		p /= m_id;
		fs::create_directories(p, ec);
		if (ec)
		{
			std::cerr << "Error in creating directory " << p << std::endl;
			finilize(false);
			return;
		}
		auto pBlob = pTrans->response().body;
		auto it = boost::asio::buffers_begin(pBlob->data());
		auto end = boost::asio::buffers_end(pBlob->data());
		std::vector<uint8_t> raw_buffer(it, end);
		auto file = p;
		file /= "mail.json" + crypt_ext;

		if (!saver.save(file, raw_buffer.data(), raw_buffer.size()))
		{
			std::cerr << "Error in saving msg to file " << file << std::endl;
			finilize(false);
			return;
		}
	}

	finilize(true);
}


AttachJob::AttachJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager)
	: Job(service, downloader, manager)
{
	m_size = 0;
}

bool AttachJob::process(const std::string& token)
{
	std::stringstream sz_str;
	sz_str << karma::format(karma::lit("/gmail/v1/users/me/messages/") << karma::string
		<< "/attachments/" << karma::string	<< "?"
		<< "key=" << karma::string
		<< "&access_token=" << karma::string,
		m_msgId, m_id, api_key, token);


	auto pTrans = m_manager.create();
	pTrans->request().verb = "GET";
	pTrans->request().path = sz_str.str();
	pTrans->request().headers["Host"] = "www.googleapis.com";

	pTrans->send(boost::bind(&AttachJob::onProcess, this, ::_1));
	return true;
}

void AttachJob::init(const std::string& id, const std::string& msgId, const std::string& filename, size_t sz)
{
	m_id = id;
	m_msgId = msgId;
	m_filename = filename;
	m_size = sz;
}


//https://tools.ietf.org/html/rfc3548#section-4
static const signed char modified_lookup_table[] =
{
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
};

template<typename Iter>
static bool modified_base64_decode(Iter& dataIt, Iter dataEnd,
																		size_t input_length,
																		uint8_t * result,
																		size_t result_length)
{

	for (int i = 0, j = 0; i < input_length;)
	{
		uint32_t a = modified_lookup_table[*dataIt++]; if (dataIt == dataEnd) return false;
		uint32_t b = modified_lookup_table[*dataIt++]; if (dataIt == dataEnd) return false;
		uint32_t c = modified_lookup_table[*dataIt++]; if (dataIt == dataEnd) return false;
		uint32_t d = modified_lookup_table[*dataIt++]; if (dataIt == dataEnd) return false;

		i += 4;

		if (a < 0 || b < 0 || c < 0 || d < 0)
			return false;

		uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

		if (j < result_length) result[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < result_length) result[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < result_length) result[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return true;
}

template<typename Iter>
bool findTagAndSkipIt(Iter& it, Iter end, const char * tag)
{
	for (; it != end; ++it) //find tag and skip it
	{
		auto p1 = it;
		const char * p2 = tag;

		for (;(p1 != end) && (*p2 != '\0') && (*p1 == *p2); ++p1, ++p2);

		if (*p2 == '\0') //we found it!
		{
			it = p1;
			break;
		}
	}
	return it != end;
}

template<typename Iter>
bool findValueAfterTag(Iter& it, Iter end)
{
	while (it != end && isspace(*it)) ++it;
	if (it != end && *it == ':') ++it; else return false;
	while (it != end && isspace(*it)) ++it;
	return it != end;
}

void AttachJob::onProcess(b::shared_ptr<HttpTransaction> pTrans)
{
	if (pTrans->response().code >= 400)
	{
		std::cerr << "Error code on downloading" << std::endl;
		finilize(false);
		return;
	}

	std::vector<uint8_t> result;

	if (!optimalReading(pTrans, result))
	{
		if (!correctReading(pTrans, result))
		{
			std::cerr << "Error in processing base64 in attachment " << std::endl;
			finilize(false);
			return;
		}
	}

	{
		CryptoSaver saver(m_downloader.getKey(), m_downloader.getRandom().get());
		b::system::error_code ec;
		fs::path p("content");
		p /= m_msgId;
		fs::create_directories(p, ec);
		if (ec)
		{
			std::cerr << "Error in creating directory " << p << std::endl;
			finilize(false);
			return;
		}
		fs::path encr_path = p / (m_filename + crypt_ext);
		fs::path orig_path = p / m_filename;
		if (fs::exists(encr_path))
		{
			for (size_t idx = 1; idx<1000; ++idx)
			{
				auto parent = orig_path.parent_path();
				auto stem = orig_path.stem().string();
				auto ext = orig_path.extension().string();

				auto newPath = parent / (stem + '(' + b::lexical_cast<std::string>(idx) + ')' + ext + crypt_ext);
				if (!fs::exists(newPath))
				{
					encr_path = newPath;
					break;
				}
			}
		}

		if (!saver.save(encr_path, result.data(), result.size()))
		{
			std::cerr << "Error in saving msg to file " << encr_path << std::endl;
			finilize(false);
			return;
		}
	}
	finilize(true);
}


bool AttachJob::optimalReading(b::shared_ptr<HttpTransaction> pTrans, std::vector<uint8_t>& result)
{
	bool res = false;
	auto buf = pTrans->response().body->data();
	auto it = bio::buffers_begin(buf);
	auto end = bio::buffers_end(buf);
	size_t buf_size;

	do
	{
		namespace qi = b::spirit::qi;

		static const char data_tag[] = "\"data\"";
		static const char size_tag[] = "\"size\"";


		if (!findTagAndSkipIt(it, end, size_tag)) break;
		if (!findValueAfterTag(it, end)) break;

		if (!qi::parse(it, end, qi::int_, buf_size)) break;

		if (!findTagAndSkipIt(it, end, data_tag)) break;
		if (!findValueAfterTag(it, end)) break;

		if (it != end && *it == '"') ++it; else break;

		res = true;
	} while (false);

	if (!res)
	{
		return false;
	}

	size_t string_sz = ((4 * buf_size / 3) + 3) & ~3;

	result.resize(buf_size);

	res = modified_base64_decode(it, end, string_sz, result.data(), result.size());
	if (!res)
		return false;

	return *it == '"';
}

bool AttachJob::correctReading(b::shared_ptr<HttpTransaction> pTrans, std::vector<uint8_t>& result)
{
	ciere::json::value attach;
	bool res = pTrans->response().getJson(attach);

	if (!res)
	{
		return false;
	}

	//read attach here
	std::string const& data = attach["data"];

	{
		if (data.size() % 4 != 0)
		{
			return false;
		}

		size_t result_size = data.size() / 4 * 3;
		for (auto it = data.rbegin(); it != data.rend(); ++it)
		{
			if (*it == '=')
				--result_size;
			else
				break;
		}

		result.resize(result_size);
		auto tmp = data.c_str();
		res = modified_base64_decode(tmp, data.c_str() + data.size(), data.size(), result.data(), result.size());
		if (!res)
			return false;

	}
	return true;

}

