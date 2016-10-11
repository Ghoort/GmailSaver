#pragma once

#include <map>
#include <string>

struct HTTPRequestHeader
{
	std::string verb;
	std::string path;
	std::string http_version;
	std::map<std::string, std::string> headers;
};

class HTTPRequest : public HTTPRequestHeader
{
public:
	HTTPRequest()
		: body(b::make_shared<boost::asio::streambuf>())
	{}

	typedef bio::buffers_iterator<bio::streambuf::mutable_buffers_type> iterator_t;

	boost::shared_ptr<boost::asio::streambuf> body;

	void prepare();
	bool generateBlob(boost::shared_ptr<boost::asio::streambuf>& pBlob);
	void createFromBlob(boost::shared_ptr<boost::asio::streambuf> pBlob);

};
