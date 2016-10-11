#pragma once

#include "HttpTransaction.h"

class HttpListener;
class Downloader;

class AuthService
{
public:
	AuthService(bio::io_service& service, TransactionManager& manager);
	~AuthService();

	void login();

	void setDownloader(Downloader * pDownloader);
private:

	void onLogin(b::shared_ptr<HttpTransaction> );
	void onListen(std::string request, bool result);
	void startRefreshTimer(const boost::system::error_code& e);
	void onRefreshTimer();
	void failed();

	bio::io_service& m_service;
	TransactionManager& m_manager;
	bio::deadline_timer m_timeout;
	b::shared_ptr<HttpListener> m_pListener;
	uint16_t m_randomPort;

	std::string access_token;
	int64_t expires_in;
	std::string token_type;
	std::string refresh_token;

	Downloader * m_pDownloader;
};
