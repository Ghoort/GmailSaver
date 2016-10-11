#include "stdafx.h"
#include "Util.h"

#include <boost/spirit/include/qi_parse.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_uint.hpp>
#include <boost/spirit/include/qi_lit.hpp>

#include <boost/asio.hpp>


bool Util::processChunkedBuffer(boost::asio::streambuf& source, boost::asio::streambuf& dst, size_t& dataToRead)
{
	while (true)
	{
		namespace qi = b::spirit::qi;
		size_t chunkLenght = 0;

		auto it = bio::buffers_begin(source.data());
		auto end = bio::buffers_end(source.data());

		if (it == end) //buffer is empty, start reading
			return true;

		bool res = qi::parse(it, end, qi::hex, chunkLenght);
		if (chunkLenght == 0) //zero sized chunk
		{
			return false;
		}
		source.consume(it - bio::buffers_begin(source.data()) + 2);

		size_t copy_bytes = bio::buffer_copy(dst.prepare(chunkLenght), source.data(), chunkLenght);

		dst.commit(copy_bytes);
		source.consume(copy_bytes + 2);

		if (copy_bytes < chunkLenght)
		{
			dataToRead = chunkLenght - copy_bytes;
			return true;
		}
	}

	return true;
}
