#pragma once

class HttpListener
{
public:
	HttpListener(bio::io_service& service);
	~HttpListener();

	uint16_t startOnRandomPort(b::function<void(std::string, bool)> handler);
	void close();
private:

	void handleAccept(const boost::system::error_code& error);
	void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
	void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);

	void handleTimeout(const boost::system::error_code& e);
	void setTimeout();
	void dropTimeout();
	void reportError();

	bio::io_service& m_service;

	boost::asio::ip::tcp::endpoint m_remote_endpoint;
	bio::ip::tcp::socket m_socket;
	bio::deadline_timer m_timeout;
	bio::strand m_strand;
	bio::ip::tcp::acceptor m_acceptor4;
	boost::shared_ptr<boost::asio::streambuf> m_inStreambuf;

	b::function<void(std::string, bool)> m_handler;

};
