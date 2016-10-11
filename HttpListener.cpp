#include "stdafx.h"
#include "HttpListener.h"
#include <random>
#include "HttpRequest.h"


static char response_txt[] =
"HTTP/1.x 200 OK\r\n"\
"Content-Type: text/html; charset=UTF-8\r\n"\
"\r\n"\
//"<html><head></head><body onload=\"self.close()\"></html>";
"<html><head><meta http-equiv='refresh' content='10;url=https://google.com'></head><body>Please return to the app.</body></html>";

static const size_t BUFF_SIZE = 1024;

HttpListener::HttpListener(bio::io_service& service)
	: m_service(service)
	, m_socket(m_service)
	, m_timeout(m_service)
	, m_strand(m_service)
	, m_acceptor4(m_service)
{

}

HttpListener::~HttpListener()
{

}

uint16_t HttpListener::startOnRandomPort(b::function<void(std::string, bool)> handler)
{
	m_handler = handler;

	uint16_t port = 0;
	b::system::error_code ec;
	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(1024, std::numeric_limits<uint16_t>::max());

	m_acceptor4.open(bio::ip::tcp::v4());

	do
	{
		port = distribution(generator);

		m_acceptor4.bind(bio::ip::tcp::endpoint(bio::ip::tcp::v4(), port), ec);
	} while (ec);

	m_acceptor4.listen();
	m_acceptor4.async_accept(m_socket,
		m_remote_endpoint,
		boost::bind(&HttpListener::handleAccept,
			this,
			boost::asio::placeholders::error)
	);

	return port;
}

void HttpListener::handleAccept(const boost::system::error_code& error)
{
	if (!error)
	{
		setTimeout();
		m_inStreambuf = boost::make_shared<boost::asio::streambuf>();
		bio::async_read_until(
			m_socket,
			*m_inStreambuf,
			"\r\n\r\n",
			m_strand.wrap(boost::bind(&HttpListener::handleRead, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));

	}
	else
	{
		std::cout << "Accept failed: " << error.message() << std::endl;
		reportError();
	}

}

void HttpListener::handleRead(const boost::system::error_code& error, size_t bytes_transferred)
{
	dropTimeout();
	if (error)
	{
		std::cout << "Read failed: " << error.message() << std::endl;
		reportError();
		return;
	}

	boost::system::error_code ec;

	m_inStreambuf->commit(bytes_transferred);

	boost::asio::async_write(m_socket,
		boost::asio::buffer(response_txt, sizeof(response_txt)),
		boost::bind(&HttpListener::handleWrite, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));

	HTTPRequest request;
	request.createFromBlob(m_inStreambuf);
	m_service.post(b::bind(m_handler, request.path, true));
}

void HttpListener::handleWrite(const boost::system::error_code& error, size_t bytes_transferred)
{
	close();
}



void HttpListener::handleTimeout(const boost::system::error_code& e)
{
	if (!e)
	{
		std::cout << "Socked timed out" << std::endl;
		reportError();
	}
}

void HttpListener::close()
{
	dropTimeout();
	boost::system::error_code ec;
	m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	m_socket.close(ec);
	m_acceptor4.close(ec);
	if (ec)
	{
		std::cout << "Close failed: " << ec.message() << std::endl;
	}
}

void HttpListener::reportError()
{

	close();

	m_service.post(b::bind(m_handler, "", false));

}

void HttpListener::setTimeout()
{
	m_timeout.expires_from_now(boost::posix_time::seconds(300));
	m_timeout.async_wait(
		m_strand.wrap(boost::bind(&HttpListener::handleTimeout,
			this,
			boost::asio::placeholders::error)));
}

void HttpListener::dropTimeout()
{
	m_timeout.cancel();
}
