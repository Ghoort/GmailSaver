#pragma once

class Util
{
public:
	static bool processChunkedBuffer(boost::asio::streambuf& source, boost::asio::streambuf& dst, size_t& dataToRead);
};
