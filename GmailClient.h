#pragma once
#include "TransactionManager.h"
#include "AuthService.h"
#include "Downloader.h"


class GmailClient
{
public:
	GmailClient();
	~GmailClient();

	bool loadCfg(int argc, const char* argv[]);
	void run();
	void stop();

private:
	bio::io_service m_service;
	b::thread_group m_threadpool;
	bio::io_service::work m_work;

	TransactionManager m_transactionManager;
	AuthService m_authService;
	Downloader m_downloader;

	std::string m_keyname;

	bpt::ptime m_start;
	bpt::ptime m_end;
};


