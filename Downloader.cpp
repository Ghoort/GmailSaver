#include "stdafx.h"
#include "Downloader.h"
#include "HttpTransaction.h"
#include "TransactionManager.h"
#include "Jobs.h"
#include "GoogleTokens.h"

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/karma.hpp>

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <ciere/json/value.hpp>



static const size_t MAX_REQUESTS = 8;


inline void assignMin(b::atomic<size_t>& a, size_t i)
{
	//m_minimalFreeMsg = i;
	size_t oldMin = a;
	while (i > oldMin)
	{
		if (a.compare_exchange_weak(oldMin, i))
			break;
		else
			oldMin = a;
	}

}

Downloader::Downloader(bio::io_service& service, TransactionManager& manager)
	: m_service(service)
	, m_manager(manager)
	, m_state(Uninitialized)
	, m_pRandom(RandomGen::create())
{
}

Downloader::~Downloader()
{

}

void Downloader::start(const std::string& token)
{
	assert(m_state == Uninitialized || m_state == Stopped);

	uint32_t state = m_state;
	m_token = token;
	m_state = Runned;

	if (state == Uninitialized)
	{
		StartProcessingList();
	}

	runJobs();
}

void Downloader::stop(b::function<void()> handler)
{
	m_state = Paused;
	m_handler = handler;
}

void Downloader::init(time_t startDate, time_t endDate, std::string const& keyname)
{
	m_startDate = startDate;
	m_endDate = endDate;
	m_key = CryptoKey::load(keyname);
}

void Downloader::StartProcessingList()
{
	std::vector<b::shared_ptr<Job> > nextJobs;
	auto p = b::make_shared<ListJob>(m_service, *this, m_manager);
	p->init(m_startDate, m_endDate);
	nextJobs.push_back(p);

	createJobs(nextJobs);
}

void Downloader::runJobs()
{
	boost::shared_lock< boost::shared_mutex > lock(m_mutex);

	while (m_state == Runned)
	{
		b::shared_ptr<Job> pMyJob;
		if (++m_requests >= MAX_REQUESTS)
		{
			--m_requests;
			return;
		}

		size_t i;
		size_t max = m_jobs.size();

		for (i = m_minimalFreeJob; i < max; ++i)
		{
			auto pJob = m_jobs[i];
			if (pJob->tryHandle())
			{
				pMyJob = pJob;
				break;
			}
		}
		assignMin(m_minimalFreeJob, i);

		if (pMyJob)
		{
			m_service.post(boost::bind(&Downloader::processJob, this, pMyJob));
		}
		else
		{
			--m_requests;
			return;
		}

		if (m_minimalFreeJob == max)
			return;
	}

}

void Downloader::processJob(b::shared_ptr<Job> pJob)
{
	if (!pJob->process(m_token))
	{
		std::cerr << "Error while processing a job" << std::endl;
		failed();
		return;
	}
}


void Downloader::failed()
{
	std::cerr << "downloading failed, closing application" << std::endl;
	m_service.stop();
}

void Downloader::successed()
{
	m_service.stop();
}

void Downloader::createJobs(std::vector<b::shared_ptr<Job> > jobs)
{
	{
		boost::unique_lock< boost::shared_mutex > lock(m_mutex);

		std::copy(std::begin(jobs), std::end(jobs), std::back_inserter(m_jobs));
	}

}

void Downloader::finilizeJob(bool res)
{
	if (!res)
	{
		std::cerr << "Error in job finalization" << std::endl;
		failed();
		return;
	}

	if (m_state == Paused)
	{
		if (--m_requests == 0)
		{
			m_state = Stopped;
			auto h = m_handler;
			m_handler.clear();
			if (h)
				m_service.post(h);
		}
		return;
	}

	if (--m_requests == 0)
	{
		boost::shared_lock< boost::shared_mutex > lock(m_mutex);
		if (m_minimalFreeJob == m_jobs.size())
		{
			//all jobs done
			successed();
			return;
		}
	}

	runJobs();
}

