#pragma once

class TcpConnection;

class ConnectionManager
{
public:
	ConnectionManager(bio::io_service& service);
	~ConnectionManager();

	b::shared_ptr<TcpConnection> getConnection();
	void freeConnection(TcpConnection*);
private:
	bio::io_service& m_service;

};
