#include "stdafx.h"
#include "ConnectionManager.h"
#include "TcpConnection.h"


ConnectionManager::ConnectionManager(bio::io_service& service)
	:  m_service(service)
{
}

ConnectionManager::~ConnectionManager()
{
}

b::shared_ptr<TcpConnection> ConnectionManager::getConnection()
{
	return b::make_shared<TcpConnection>(m_service, this);
}

void ConnectionManager::freeConnection(TcpConnection* pConnection)
{
}
