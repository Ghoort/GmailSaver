#include "stdafx.h"

#include "GmailClient.h"

#include "vld.h"

int main(int argc, const char *argv[])
{
	try
	{
		GmailClient client;
		if (!client.loadCfg(argc, argv))
			return 0;
		client.run();
	}
	catch (boost::exception& e)
	{
		std::cerr << "Server failed to start due to " << (boost::diagnostic_information(e)) << std::endl;
	}
	catch (std::exception& e)
	{
		std::cerr << "Server failed to start due to " << e.what() << std::endl;
	}

	return 0;
}

