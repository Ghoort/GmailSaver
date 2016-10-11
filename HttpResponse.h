#pragma once

namespace ciere { namespace json { class value; } }

struct HTTPResponseHeader
{
	int code;
	std::map<std::string, std::string> headers;

};


class HTTPResponse : public HTTPResponseHeader
{
public:
	boost::shared_ptr<boost::asio::streambuf> body;

	void createFromBlob(boost::shared_ptr<boost::asio::streambuf> pBlob);

	bool getJson(ciere::json::value& json_val);
};


