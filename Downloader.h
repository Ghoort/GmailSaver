#pragma once

#include "CryptoSaver.h"

class TransactionManager;
class HttpTransaction;

class Job;

class Downloader
{
public:
	Downloader(bio::io_service& service, TransactionManager& manager);
	~Downloader();

	void start(const std::string& token);
	void stop(b::function<void()> handler);
	void init(time_t startDate, time_t endDate, std::string const& keyname);

	void createJobs(std::vector<b::shared_ptr<Job> > jobs);
	void finilizeJob(bool res);

	CryptoKey getKey();
	b::shared_ptr<RandomGen> getRandom();

private:
	enum  State
	{
		Uninitialized, Runned, Paused, Stopped
	};
	void StartProcessingList();

	void runJobs();
	void processJob(b::shared_ptr<Job> pJob);

	void failed();
	void successed();

	bio::io_service& m_service;
	TransactionManager& m_manager;

	b::shared_mutex m_mutex;
	std::deque<b::shared_ptr<Job> > m_jobs;

	b::atomic<size_t> m_minimalFreeJob;
	b::atomic<size_t> m_requests;
	b::atomic<uint32_t> m_state;

	std::string m_token;

	time_t m_startDate;
	time_t m_endDate;

	CryptoKey m_key;
	b::shared_ptr<RandomGen> m_pRandom;

	b::function<void()> m_handler;
};



inline CryptoKey Downloader::getKey()
{
	return m_key;
}

inline b::shared_ptr<RandomGen> Downloader::getRandom()
{
	return m_pRandom;
}
