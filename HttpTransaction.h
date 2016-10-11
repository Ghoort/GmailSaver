#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "TcpConnection.h"

class TransactionManager;

class HttpTransaction : public b::enable_shared_from_this<HttpTransaction>
{
public:

	HttpTransaction(bio::io_service& service, TransactionManager * pManager);
	~HttpTransaction();

	HTTPRequest& request();
	HTTPResponse& response();
	void send(b::function<void(b::shared_ptr<HttpTransaction>)> completeHander);
private:
	void startSending();
	void onSend(boost::shared_ptr<boost::asio::streambuf>, bool);
	void onSend2(boost::shared_ptr<boost::asio::streambuf>, bool);
	void failedTransaction();

	HTTPRequest m_request;
	HTTPResponse m_response;

	bio::io_service& m_service;
	TransactionManager * m_pManager;

	b::function<void(b::shared_ptr<HttpTransaction>)> m_completeHander;
	b::shared_ptr<TcpConnection> m_pConnection;
	size_t m_attempts;
	boost::shared_ptr<boost::asio::streambuf> m_requestBlob;
};


inline HTTPRequest& HttpTransaction::request()
{
	return m_request;
}

inline HTTPResponse& HttpTransaction::response()
{
	return m_response;
}



