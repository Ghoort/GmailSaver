#include "stdafx.h"

#include "GmailClient.h"

#include <boost/program_options.hpp>
#include <boost/locale.hpp>

namespace po = b::program_options;


GmailClient::GmailClient()
	: m_work(m_service)
	, m_transactionManager(m_service)
	, m_authService(m_service, m_transactionManager)
	, m_downloader(m_service, m_transactionManager)
{
	std::locale::global(boost::locale::generator().generate("en_US.UTF-8"));
	fs::path::imbue(std::locale());
	m_authService.setDownloader(&m_downloader);
}

GmailClient::~GmailClient()
{
}

bool GmailClient::loadCfg(int argc, const char* argv[])
{
	po::options_description desc;
	std::string cfg_filename;
	std::string startDate;
	std::string endDate;


	po::options_description general_desc("General options");
	general_desc.add_options()
		("help,h", "Show help")
		("cfgfile,c", po::value<std::string>(&cfg_filename), "Cfg file name")
		("key,k", po::value<std::string>(&m_keyname), "Key")
		("start,s", po::value<std::string>(&startDate), "Start date")
		("end,e", po::value<std::string>(&endDate), "End date")
		;

	desc.add(general_desc);

	po::variables_map vm;
	po::parsed_options parsedline = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
	po::store(parsedline, vm);
	po::notify(vm);
	if (vm.count("help"))
	{
		std::cout << desc << std::endl;
		return false;
	}

	if (!cfg_filename.empty())
	{
		fs::ifstream file(cfg_filename);
		if (file)
		{
			po::store(po::parse_config_file(file, desc), vm);
		}
		else
		{
			std::cout << L"Could not load configuration file " << cfg_filename << L". Exiting." << std::endl;
			return false;
		}
	}
	else
	{
		fs::ifstream file("gmail.cfg");
		po::store(po::parse_config_file(file, desc), vm);
	}
	po::store(parsedline, vm); //overwrite data from file
	po::notify(vm);

	if (startDate.empty() || endDate.empty() || m_keyname.empty())
	{
		std::cout << desc << std::endl;
		return false;
	}

	m_start = bpt::time_from_string(startDate);
	m_end = bpt::time_from_string(endDate);

	{
		bpt::ptime epoch(boost::gregorian::date(1970, 1, 1));
		m_downloader.init((m_start - epoch).total_seconds(), (m_end - epoch).total_seconds(), m_keyname);
	}

	return true;
}

void GmailClient::run()
{
	try
	{
		for (size_t i = 0; i < 6; ++i)
			m_threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &m_service));

		m_authService.login();
	}
	catch (boost::exception& e)
	{
		std::cerr << "Server failed to start due to " << (boost::diagnostic_information(e)) << std::endl;
		stop();
	}
	catch (std::exception& e)
	{
		std::cerr << "Server failed to start due to " << e.what() << std::endl;
		stop();
	}
	m_threadpool.join_all();

}

void GmailClient::stop()
{
	std::wcout << L"Exiting...." << std::endl;

}
