#pragma once

#include <boost/asio/ssl.hpp>

class ConnectionManager;

class TcpConnection
{
public:
	TcpConnection(bio::io_service& service, ConnectionManager * pManager);
	~TcpConnection();

	void setCompletingHandler(b::function<void(boost::shared_ptr<boost::asio::streambuf>, bool)>);
	void send(std::string const& server, boost::shared_ptr<boost::asio::streambuf> data);
	void readBytes(boost::shared_ptr<boost::asio::streambuf> data, size_t bytes);
	void readChunks(boost::shared_ptr<boost::asio::streambuf> data);

private:
	void handleResolve(const boost::system::error_code& err, bio::ip::tcp::resolver::iterator endpoint_iterator);
	void handleConnect(const boost::system::error_code& err);
	void handleHandshake(const boost::system::error_code& error);
	void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);
	void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
	void handleRead2(const boost::system::error_code& error, size_t bytes_transferred);
	void readNextChunk();
	void handleReadChunks(size_t myBytes, const boost::system::error_code& error, size_t bytes_transferred);

	void reportError();
	void close();
	void handleTimeout(const boost::system::error_code& e);
	void setTimeout();
	void dropTimeout();

	bio::io_service& m_service;
	ConnectionManager * m_pManager;
	bio::ssl::context m_context;
	bio::deadline_timer m_timeout;
	bio::strand m_strand;


	static std::atomic<size_t> counter;
	size_t id;

	bio::ip::tcp::resolver m_resolver;
	bio::ssl::stream<bio::ip::tcp::socket> m_socket;
	boost::shared_ptr<boost::asio::streambuf> m_inStreambuf;
	boost::shared_ptr<boost::asio::streambuf> m_outStreambuf;

	boost::shared_ptr<boost::asio::streambuf> m_tmpStreambuf;

	b::function<void(boost::shared_ptr<boost::asio::streambuf>, bool)> m_handler;
};
