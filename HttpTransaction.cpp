#include "stdafx.h"
#include "HttpTransaction.h"
#include "TransactionManager.h"

#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_numeric.hpp>

HttpTransaction::HttpTransaction(bio::io_service& service, TransactionManager * pManager)
	: m_service(service)
	, m_pManager(pManager)
	, m_attempts(0)
{

}

HttpTransaction::~HttpTransaction()
{

}

void HttpTransaction::send(b::function<void(b::shared_ptr<HttpTransaction>)> completeHander)
{
	m_completeHander = completeHander;
	m_request.prepare();
	if (!m_request.generateBlob(m_requestBlob))
	{
		std::cerr << "Send failed " << std::endl;
		failedTransaction();
		return;
	}
	startSending();
}

void HttpTransaction::startSending()
{
	m_pConnection = m_pManager->getConnectionManager().getConnection();

	std::string server = m_request.headers["Host"];

	m_pConnection->setCompletingHandler(b::bind(&HttpTransaction::onSend, shared_from_this(), _1, _2));
	m_pConnection->send(server, m_requestBlob);
}

void HttpTransaction::onSend(boost::shared_ptr<boost::asio::streambuf> blob, bool res)
{
	if (!res)
	{
		if (++m_attempts < 10)
		{
			startSending();
		}
		else
		{
			failedTransaction();
		}
	}
	else
	{
		m_response.createFromBlob(blob);

		if (m_response.headers.count("Content-Length"))
		{
			std::string contentLenghtTxt = m_response.headers["Content-Length"];
			int contentLenght = 0;
			boost::spirit::qi::parse(contentLenghtTxt.begin(), contentLenghtTxt.end(), boost::spirit::qi::int_, contentLenght);
			size_t body_size = bio::buffer_size(m_response.body->data());
			if (contentLenght > body_size)
			{
				m_pConnection->setCompletingHandler(b::bind(&HttpTransaction::onSend2, shared_from_this(), _1, _2));
				m_pConnection->readBytes(m_response.body, contentLenght - body_size);
				return;
			}
		}
		else if(m_response.headers.count("Transfer-Encoding"))
		{
			std::string encoding = m_response.headers["Transfer-Encoding"];
			if (encoding == "chunked")
			{
				m_pConnection->setCompletingHandler(b::bind(&HttpTransaction::onSend2, shared_from_this(), _1, _2));
				m_pConnection->readChunks(m_response.body);
				return;
			}
		}

		m_requestBlob = nullptr;
		m_pConnection = nullptr;
		m_service.post(b::bind(m_completeHander, shared_from_this()));
	}
}

void HttpTransaction::onSend2(boost::shared_ptr<boost::asio::streambuf> pRes, bool res)
{
	if (!res)
	{
		if (++m_attempts < 10)
		{
			startSending();
		}
		else
		{
			failedTransaction();
		}
		return;
	}
	m_response.body = pRes;
	m_requestBlob = nullptr;
	m_pConnection = nullptr;
	m_service.post(b::bind(m_completeHander, shared_from_this()));
}


void HttpTransaction::failedTransaction()
{
	std::cerr << "Stopping io_service due to transaction failure" << std::endl;
	m_service.stop();
}
