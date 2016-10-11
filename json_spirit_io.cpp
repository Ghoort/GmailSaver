#include "stdafx.h"

#include <ciere/json/value.hpp>
#include <ciere/json/parser/grammar_def.hpp>

#include <boost/asio.hpp>

template struct ciere::json::parser::grammar<boost::asio::buffers_iterator<class boost::asio::const_buffers_1, char> >;
