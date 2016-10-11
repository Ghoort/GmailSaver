#include "stdafx.h"
#include "AuthService.h"
#include "TransactionManager.h"
#include "HttpListener.h"
#include "Downloader.h"
#include "GoogleTokens.h"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>

#include <ciere/json/value.hpp>

namespace bs = b::spirit;


AuthService::AuthService(bio::io_service& service, TransactionManager& manager)
	: m_service(service)
	, m_manager(manager)
	, m_timeout(service)
	, m_randomPort(0)
{

}

AuthService::~AuthService()
{

}

void AuthService::login()
{
	namespace karma = bs::karma;
	m_pListener = b::make_shared<HttpListener>(m_service);

	m_randomPort = m_pListener->startOnRandomPort(boost::bind(&AuthService::onListen, this, _1, _2));

	std::stringstream sz_str;
	sz_str << karma::format(karma::lit("https://accounts.google.com/o/oauth2/v2/auth")
		<< "?response_type=code"
		<< "&scope=https://www.googleapis.com/auth/gmail.readonly"
		<< "&redirect_uri=http://127.0.0.1:" << karma::int_ 
		<< "&client_id=" << karma::string
		, m_randomPort, client_id);


	std::string req = sz_str.str();

	//start req in browser (windows specific code here!)
	system(("start explorer \"" + req + "\"").c_str());
}

void AuthService::onLogin(b::shared_ptr<HttpTransaction> pAnswer)
{
	if (pAnswer->response().code >= 400)
	{
		std::cerr << "Error code on authorization request" << std::endl;
		failed();
		return;
	}

	ciere::json::value json_val;

	bool res = pAnswer->response().getJson(json_val);

	if (!res)
	{
		std::cerr << "Error on processing authorization request" << std::endl;
		failed();
		return;
	}

	access_token = json_val["access_token"];
	expires_in = json_val["expires_in"].get<int64_t>();
	token_type = json_val["token_type"];
	refresh_token = json_val["refresh_token"];

	m_timeout.expires_from_now(bpt::seconds((long)expires_in-30));
	m_timeout.async_wait(
		boost::bind(&AuthService::startRefreshTimer,
			this,
			boost::asio::placeholders::error));

	if (m_pDownloader)
		m_pDownloader->start(access_token);
}

void AuthService::startRefreshTimer(const boost::system::error_code& e)
{
	if (e)
		return;

	if (m_pDownloader)
		m_pDownloader->stop(boost::bind(&AuthService::onRefreshTimer,	this));
}

void AuthService::onRefreshTimer()
{
	namespace karma = bs::karma;

	auto pTrans = m_manager.create();
	pTrans->request().verb = "POST";
	pTrans->request().path = "/oauth2/v4/token";
	pTrans->request().headers["Host"] = "www.googleapis.com";
	pTrans->request().headers["Content-Type"] = "application/x-www-form-urlencoded";

	auto mutable_buf = pTrans->request().body->prepare(1024);
	auto it = bio::buffers_begin(mutable_buf);

	bool res = karma::generate(it,
		"client_id=" << karma::string << '&' <<
		"client_secret=" << karma::string << '&' <<
		"refresh_token=" << karma::string << '&' <<
		"grant_type=refresh_token",
		client_id, client_secret, refresh_token
	);

	if (!res)
	{
		std::cerr << "Error on creating authorization request" << std::endl;
		failed();
		return;
	}

	pTrans->request().body->commit(it - bio::buffers_begin(mutable_buf));
	pTrans->request().prepare();

	pTrans->send(boost::bind(&AuthService::onLogin, this, ::_1));

}




void AuthService::onListen(std::string request, bool resultOfRequest)
{
	namespace karma = bs::karma;
	namespace qi = bs::qi;

	if (!resultOfRequest)
	{
		std::cerr << "Error on receiving authorization request" << std::endl;
		failed();
		return;
	}
	std::string result;
	std::string code;

	auto res = qi::parse(std::begin(request), std::end(request),
			qi::lit("/?") >> *(qi::char_("a-zA-Z")) || ('=' >> *qi::char_), 
			result, code);

	if (!res)
	{
		std::cerr << "Error on parsing authorization request" << std::endl;
		failed();
		return;
	}

	if (result != "code" || code.empty())
	{
		std::cerr << "Did not authorize" << std::endl;
		failed();
		return;
	}


	auto pTrans = m_manager.create();
	pTrans->request().verb = "POST";
	pTrans->request().path = "/oauth2/v4/token";
	pTrans->request().headers["Host"] = "www.googleapis.com";
	pTrans->request().headers["Content-Type"] = "application/x-www-form-urlencoded";

	auto mutable_buf = pTrans->request().body->prepare(1024);
	auto it = bio::buffers_begin(mutable_buf);

	res = karma::generate(it,
		"code=" << karma::string << '&' << 
		"client_id=" << karma::string << '&' <<
		"client_secret=" << karma::string << '&' <<
		"redirect_uri=http%3A%2F%2F127.0.0.1" << "%3A" << karma::int_ << '&' <<
		"grant_type=authorization_code",
		code, client_id, client_secret, m_randomPort
	);

	if (!res)
	{
		std::cerr << "Error on creating authorization request" << std::endl;
		failed();
		return;
	}

	pTrans->request().body->commit(it - bio::buffers_begin(mutable_buf));
	pTrans->request().prepare();

	pTrans->send(boost::bind(&AuthService::onLogin, this, ::_1));
}

void AuthService::failed()
{
	std::cerr << "authorization failed, closing application" << std::endl;
	m_service.stop();
}


void AuthService::setDownloader(Downloader * pDownloader)
{
	m_pDownloader = pDownloader;
}
