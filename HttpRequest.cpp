#include "stdafx.h"
#include "HttpRequest.h"



#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp> 
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>


BOOST_FUSION_ADAPT_STRUCT(
	HTTPRequestHeader,
	verb,
	path,
	http_version,
	headers
)

namespace qi = boost::spirit::qi;

template <typename OutputIterator>
struct HttpKarmGrammar : boost::spirit::karma::grammar<OutputIterator, HTTPRequestHeader() >
{
	HttpKarmGrammar() : HttpKarmGrammar::base_type(msg_rule)
	{
		using namespace boost::spirit;

		map_rule = (karma::string << ": " << karma::string) % "\r\n";
		msg_rule =
			karma::string << ' ' << karma::string << " HTTP/" << karma::string << "\r\n"
			 << map_rule <<	"\r\n\r\n";

		//karma::debug(map_rule);
		//karma::debug(msg_rule);

	}

	boost::spirit::karma::rule<OutputIterator, std::map<std::string, std::string>()> map_rule;
	boost::spirit::karma::rule<OutputIterator, HTTPRequestHeader()> msg_rule;
};

bool HTTPRequest::generateBlob(boost::shared_ptr<boost::asio::streambuf>& pBlob)
{
	using namespace boost::spirit;
	using namespace boost::spirit::karma;
	typedef bio::buffers_iterator<bio::streambuf::mutable_buffers_type> iterator_t;

	pBlob = b::make_shared<boost::asio::streambuf>();
	bio::streambuf::mutable_buffers_type mutablebufs(pBlob->prepare(1024));

	iterator_t outiter = iterator_t::begin(mutablebufs);

	HttpKarmGrammar<iterator_t> grammar;

	if (!generate(outiter, grammar, *this))
	{
		std::cerr << "Header generation failed " << std::endl;
		return false;
	}

	pBlob->commit(outiter - bio::buffers_begin(mutablebufs));

	size_t sz = bio::buffer_size(body->data());
	if (sz)
	{
		bio::buffer_copy(pBlob->prepare(sz), body->data());
		pBlob->commit(sz);
	}

	return true;
}

void HTTPRequest::prepare()
{
	using namespace b::spirit;

	if (http_version.empty())
		http_version = "1.1";

	size_t sz = bio::buffer_size(body->data());
	std::stringstream sz_str;
	sz_str << karma::format(karma::int_, sz);

	headers["Content-Length"] = sz_str.str();
}


template <typename Iterator, typename Skipper = qi::ascii::blank_type>
struct HttpQiGrammar : boost::spirit::qi::grammar<Iterator, HTTPRequestHeader(), Skipper >
{
	HttpQiGrammar() : HttpQiGrammar::base_type(http_header)
	{
		using namespace boost::spirit;

		method = +qi::alpha;
		uri = +qi::graph;
		http_ver = "HTTP/" >> +qi::char_("0-9.");

		field_key = +qi::char_("0-9a-zA-Z-");
		field_value = +~qi::char_("\r\n");

		fields = *(field_key >> ':' >> field_value >> qi::lexeme["\r\n"]);

		http_header = method >> uri >> http_ver >> qi::lexeme["\r\n"] >> fields;

	}

	qi::rule<Iterator, std::map<std::string, std::string>(), Skipper> fields;
	qi::rule<Iterator, HTTPRequestHeader(), Skipper> http_header;
	// lexemes
	qi::rule<Iterator, std::string()> method, uri, http_ver;
	qi::rule<Iterator, std::string()> field_key, field_value;
};


void HTTPRequest::createFromBlob(boost::shared_ptr<boost::asio::streambuf> pBlob)
{
	auto it = boost::asio::buffers_begin(pBlob->data());
	auto end = boost::asio::buffers_end(pBlob->data());
	HttpQiGrammar<decltype(it)> grammar;
	HTTPRequestHeader header;

	bool res = boost::spirit::qi::phrase_parse(it, end, grammar, boost::spirit::qi::ascii::blank, header);

	if (!res)
	{
		std::string non_parsed(it, end);
		std::cerr << "Non parsed:\n" << non_parsed;
	}
	size_t sz = it - boost::asio::buffers_begin(pBlob->data());
	pBlob->consume(sz);
	body = pBlob;
	path = std::move(header.path);
	verb = std::move(header.verb);
	headers = std::move(header.headers);

}


