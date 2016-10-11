#include "stdafx.h"
#include "TransactionManager.h"


TransactionManager::TransactionManager(bio::io_service& service)
	: m_service(service)
	, m_connectionManager(service)
{
}

TransactionManager::~TransactionManager()
{

}

b::shared_ptr<HttpTransaction> TransactionManager::create()
{
	return b::make_shared<HttpTransaction>(m_service, this);
}
