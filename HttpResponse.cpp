#include "stdafx.h"
#include "HttpResponse.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp> 
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/map.hpp>

#include <ciere/json/value.hpp>
#include <ciere/json/io.hpp>



BOOST_FUSION_ADAPT_STRUCT(
	HTTPResponseHeader,
	code,
	headers
)

namespace bs = boost::spirit;
namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;

template <typename Iterator, typename Skipper = qi::ascii::blank_type>
struct HttpGrammar : boost::spirit::qi::grammar<Iterator, HTTPResponseHeader(), Skipper >
{
	HttpGrammar() : HttpGrammar::base_type(http_header)
	{
		http_ver = "HTTP/" >> +qi::char_("0-9.");

		field_key = +qi::char_("0-9a-zA-Z-");
		field_value = +~qi::char_("\r\n");

		fields = *(field_key >> ':' >> field_value >> qi::lexeme["\r\n"]);

		http_header = qi::omit[http_ver] >> qi::int_ >> qi::omit[*qi::graph] >> qi::lexeme["\r\n"] 
			>> fields >> qi::lexeme["\r\n"];

	}

	qi::rule<Iterator, std::map<std::string, std::string>(), Skipper> fields;
	qi::rule<Iterator, HTTPResponseHeader(), Skipper> http_header;
	qi::rule<Iterator, std::string()> http_ver;
	qi::rule<Iterator, std::string()> field_key, field_value;
};

void HTTPResponse::createFromBlob(boost::shared_ptr<boost::asio::streambuf> pBlob)
{

	auto it = boost::asio::buffers_begin(pBlob->data());
	auto end = boost::asio::buffers_end(pBlob->data());
	HttpGrammar<decltype(it)> grammar;
	HTTPResponseHeader header;
	bool res = phrase_parse(it, end, grammar, qi::ascii::blank, header);

	if (!res)
	{
		std::string non_parsed(it, end);
		std::cerr << "Non parsed:\n" << non_parsed;
	}
	size_t sz = it - boost::asio::buffers_begin(pBlob->data());
	pBlob->consume(sz);
	body = pBlob;
	code = header.code;
	headers = std::move(header.headers);
}


bool HTTPResponse::getJson(ciere::json::value& json_val)
{
	auto it = boost::asio::buffers_begin(body->data());
	auto end = boost::asio::buffers_end(body->data());


	bool res = ciere::json::construct(it, end, json_val);
	return res;
}
