#include "stdafx.h"
#include "TcpConnection.h"
#include "ConnectionManager.h"
#include "Util.h"


using bio::ip::tcp;
static const size_t BUFF_SIZE = 2048;


std::atomic<size_t> TcpConnection::counter;

TcpConnection::TcpConnection(bio::io_service& service, ConnectionManager * pManager)
	: m_service(service)
	, m_pManager(pManager)
	, m_context(bio::ssl::context::sslv23)
	, m_timeout(m_service)
	, m_strand(m_service)
	, m_resolver(m_service)
	, m_socket(m_service, m_context)
{
	m_context.set_verify_mode(boost::asio::ssl::context::verify_none);
	m_context.set_default_verify_paths();
	//context.load_verify_file("certificate.pem");

	id = ++counter;
}

TcpConnection::~TcpConnection()
{
	close();
	m_pManager->freeConnection(this);
}

void TcpConnection::setCompletingHandler(b::function<void(boost::shared_ptr<boost::asio::streambuf>, bool)> handler)
{
	m_handler = handler;
}

void TcpConnection::send(std::string const& server, boost::shared_ptr<boost::asio::streambuf> data)
{
	m_outStreambuf = data;

	setTimeout();
	tcp::resolver::query tcp_query(server, "443");
	m_resolver.async_resolve(tcp_query,
		m_strand.wrap(boost::bind(&TcpConnection::handleResolve, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::iterator)));

}

void TcpConnection::setTimeout()
{
	m_timeout.expires_from_now(boost::posix_time::seconds(300));
	m_timeout.async_wait(
		m_strand.wrap(boost::bind(&TcpConnection::handleTimeout,
			this,
			boost::asio::placeholders::error)));
}

void TcpConnection::dropTimeout()
{
	m_timeout.cancel();
}


void TcpConnection::handleResolve(const boost::system::error_code& err,
	bio::ip::tcp::resolver::iterator endpoint_iterator)
{
	dropTimeout();
	if (!err)
	{
		setTimeout();
		boost::asio::async_connect(m_socket.lowest_layer(), endpoint_iterator,
			m_strand.wrap(boost::bind(&TcpConnection::handleConnect, this,
				boost::asio::placeholders::error)));
	}
	else
	{
		//std::cout << "Error: " << err.message() << std::endl;
		reportError();
	}
}

void TcpConnection::handleConnect(const boost::system::error_code& err)
{
	dropTimeout();
	if (!err)
	{
		setTimeout();
		m_socket.async_handshake(boost::asio::ssl::stream_base::client,
			m_strand.wrap(boost::bind(&TcpConnection::handleHandshake, this,
				boost::asio::placeholders::error)));
	}
	else
	{
		//std::cout << "Connect failed: " << err.message() << std::endl;
		reportError();
	}

}

void TcpConnection::handleHandshake(const boost::system::error_code& error)
{
	dropTimeout();
	if (!error)
	{
		//auto it = boost::asio::buffers_begin(m_outStreambuf->data());
		//auto end = boost::asio::buffers_end(m_outStreambuf->data());

		//std::cout << "Sending request:\n" << std::string(it, end) << "\nEnd of request" << std::endl;

		setTimeout();
		boost::asio::async_write(m_socket,
			m_outStreambuf->data(),
			m_strand.wrap(boost::bind(&TcpConnection::handleWrite, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));
	}
	else
	{
		//std::cout << "Handshake failed: " << error.message() << std::endl;
		reportError();
	}

}


void TcpConnection::handleWrite(const boost::system::error_code& error, size_t bytes_transferred)
{
	dropTimeout();
	if (!error)
	{
		m_inStreambuf = boost::make_shared<boost::asio::streambuf>();
		setTimeout();

		m_socket.async_read_some(
			m_inStreambuf->prepare(BUFF_SIZE),
			m_strand.wrap(boost::bind(&TcpConnection::handleRead, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));

	}
	else
	{
		//std::cout << "Write failed: " << error.message() << std::endl;
		reportError();
	}

}


void TcpConnection::handleRead(const boost::system::error_code& error, size_t bytes_transferred)
{
	dropTimeout();
	if(error)
	{
		//std::cout << "Read failed: " << error.message() << std::endl;
		reportError();
		return;
	}

	boost::system::error_code ec;

	while (bytes_transferred == BUFF_SIZE && !ec)
	{
		m_inStreambuf->commit(BUFF_SIZE);
		bytes_transferred = m_socket.read_some(m_inStreambuf->prepare(BUFF_SIZE), ec);
	}
	m_inStreambuf->commit(bytes_transferred);

	{
	//	auto it = boost::asio::buffers_begin(m_inStreambuf->data());
	//	auto end = boost::asio::buffers_end(m_inStreambuf->data());

	//	std::cout << "Received response:\n" << std::string(it, end) << "\nEnd of response" << std::endl;
	}

	auto tmp = m_inStreambuf;
	m_inStreambuf = nullptr;
	m_service.post(b::bind(m_handler, tmp, true));
}

void TcpConnection::readChunks(boost::shared_ptr<boost::asio::streambuf> data)
{
	m_tmpStreambuf = data;
	m_inStreambuf = boost::make_shared<boost::asio::streambuf>();

	readNextChunk();
}

void TcpConnection::readNextChunk()
{
	size_t szToRead = 0;
	bool nonEmpty = Util::processChunkedBuffer(*m_tmpStreambuf, *m_inStreambuf, szToRead);
	if (nonEmpty)
	{
		setTimeout();

		m_socket.async_read_some(
			m_tmpStreambuf->prepare(BUFF_SIZE),
			m_strand.wrap(boost::bind(&TcpConnection::handleReadChunks, this, szToRead,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));

	}
	else
	{
		auto tmp = m_inStreambuf;
		m_inStreambuf = nullptr;
		m_service.post(b::bind(m_handler, tmp, true));
	}
}


void TcpConnection::readBytes(boost::shared_ptr<boost::asio::streambuf> data, size_t bytes)
{
	m_inStreambuf = data;
	setTimeout();
	bio::async_read(
		m_socket,
		m_inStreambuf->prepare(bytes),
		m_strand.wrap(boost::bind(&TcpConnection::handleRead2, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)));
}

void TcpConnection::handleRead2(const boost::system::error_code& error, size_t bytes_transferred)
{
	dropTimeout();
	if (error)
	{
		//std::cout << "Read failed: " << error.message() << std::endl;
		reportError();
		return;
	}
	m_inStreambuf->commit(bytes_transferred);

	auto tmp = m_inStreambuf;
	m_inStreambuf = nullptr;
	m_service.post(b::bind(m_handler, tmp, true));
}

void TcpConnection::handleReadChunks(size_t myBytes, const boost::system::error_code& error, size_t bytes_transferred)
{
	dropTimeout();
	if (error)
	{
		//std::cout << "Read failed: " << error.message() << std::endl;
		reportError();
		return;
	}
	m_tmpStreambuf->commit(bytes_transferred);
	
	size_t bytes_load = std::min(bytes_transferred, myBytes);
	size_t copied_bytes = bio::buffer_copy(m_inStreambuf->prepare(bytes_load), m_tmpStreambuf->data(), bytes_load);
	m_inStreambuf->commit(copied_bytes);
	m_tmpStreambuf->consume(copied_bytes);

	if (bytes_transferred < myBytes)
	{
		setTimeout();

		m_socket.async_read_some(
			m_tmpStreambuf->prepare(BUFF_SIZE),
			m_strand.wrap(boost::bind(&TcpConnection::handleReadChunks, this, myBytes - bytes_transferred,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));

	}
	else
	{
		if (myBytes > 0)//we are at the end of large chunk, we should skip its ending
			m_tmpStreambuf->consume(2);
		readNextChunk();
	}
}


void TcpConnection::close()
{
	dropTimeout();
	boost::system::error_code ec;
	m_socket.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	m_socket.lowest_layer().close(ec);
	if (ec)
	{
		//std::cout << "Close failed: " << ec.message() << std::endl;
	}
}

void TcpConnection::reportError()
{

	m_inStreambuf = nullptr;
	m_outStreambuf = nullptr;

	close();

	m_service.post(b::bind(m_handler, m_inStreambuf, false));
}

void TcpConnection::handleTimeout(const boost::system::error_code& e)
{
	if (!e)
	{
		//std::cout << "Socked timed out" << std::endl;
		reportError();
	}
}
