
#pragma once

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define BOOST_SPIRIT_USE_PHOENIX_V3 
#define BOOST_SPIRIT_UNICODE
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#ifdef _DEBUG
//#define BOOST_SPIRIT_KARMA_DEBUG
//#define BOOST_SPIRIT_QI_DEBUG
#endif

#include <boost/asio.hpp>
#include <boost/bind.hpp>	
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <boost/shared_array.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <boost/variant.hpp>
#include <boost/utility/string_ref.hpp> 

#include <boost/lockfree/queue.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <iosfwd>
#include  <fstream>
#include <set>
#include <stdio.h>

namespace b = ::boost;
namespace bio = b::asio;
namespace fs = b::filesystem;
namespace bpt = b::posix_time;

