#pragma once

#include "HttpTransaction.h"
#include "ConnectionManager.h"

class TransactionManager
{
public:
	TransactionManager(bio::io_service& service);
	~TransactionManager();

	b::shared_ptr<HttpTransaction> create();

	ConnectionManager& getConnectionManager();
private:
	bio::io_service& m_service;
	ConnectionManager m_connectionManager;
};

inline ConnectionManager& TransactionManager::getConnectionManager()
{
	return m_connectionManager;
}
