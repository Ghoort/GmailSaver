#pragma once

class Downloader;
class TransactionManager;
class HttpTransaction;

class Job
{
public:
	Job(bio::io_service& service, Downloader& downloader, TransactionManager& manager);
	virtual ~Job();

	virtual bool process(const std::string& token) = 0;
	bool tryHandle();
protected:
	void finilize(bool res);

	bio::io_service& m_service;
	Downloader& m_downloader;
	TransactionManager& m_manager;

	b::atomic_flag m_free;

};


class ListJob : public Job
{
public:
	ListJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager);
	virtual bool process(const std::string& token);

	void init(time_t startDate, time_t endDate, std::string const& nextPage = "");
private:
	void onProcess(b::shared_ptr<HttpTransaction> pTrans);

	std::string m_nextPage;
	time_t m_startDate;
	time_t m_endDate;

};


class MsgJob : public Job
{
public:
	MsgJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager);
	virtual bool process(const std::string& token);

	void init(const std::string& id);

private:
	void onProcess(b::shared_ptr<HttpTransaction> pTrans);

	std::string m_id;
};



class AttachJob : public Job
{
public:
	AttachJob(bio::io_service& service, Downloader& downloader, TransactionManager& manager);
	virtual bool process(const std::string& token);

	void init(const std::string& id, const std::string& msgId, const std::string& filename, size_t sz);

private:
	bool optimalReading(b::shared_ptr<HttpTransaction> pTrans, std::vector<uint8_t>& result);
	bool correctReading(b::shared_ptr<HttpTransaction> pTrans, std::vector<uint8_t>& result);

	void onProcess(b::shared_ptr<HttpTransaction> pTrans);

	std::string m_id;
	std::string m_msgId;
	std::string m_filename;
	size_t m_size;
};



inline bool Job::tryHandle()
{
	return !m_free.test_and_set();
}


