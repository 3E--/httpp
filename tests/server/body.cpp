/*
 * Part of HTTPP.
 *
 * Distributed under the 3-clause BSD licence (See LICENCE.TXT file at the
 * project root).
 *
 * Copyright (c) 2013 Thomas Sanchez.  All rights reserved.
 *
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <istream>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "httpp/HttpServer.hpp"
#include "httpp/utils/Exception.hpp"

using namespace HTTPP;

using HTTPP::HTTP::Request;
using HTTPP::HTTP::Response;
using HTTPP::HTTP::Connection;

#define BODY_SIZE 8092

static const std::string REQUEST =
    "POST / HTTP/1.1\r\n"
    "User-Agent: curl/7.32.0\r\n"
    "Host: localhost:8000\r\n"
    "Accept: */*\r\n"
    "Content-Length: " BOOST_PP_STRINGIZE(BODY_SIZE) "\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "\r\n";

std::string BODY;

static Connection* gconnection = nullptr;
void body_handler(const boost::system::error_code& ec, const char* buffer, size_t n)
{
    static std::string body_read;

    std::cout << "Received body part" << std::endl;

    if (ec == boost::asio::error::eof)
    {
        std::cout << "Check" << std::endl;
        BOOST_CHECK_EQUAL(body_read, BODY);
        (gconnection->response() = Response(HTTP::HttpCode::Ok)).connectionShouldBeClosed(true);
        gconnection->sendResponse();
    }
    else if (ec)
    {
        throw HTTPP::UTILS::convert_boost_ec_to_std_ec(ec);
    }
    else
    {
        body_read.append(buffer, n);
    }
}

void handler(Connection* connection, Request&& request)
{
    gconnection = connection;
    std::cout << "got a request" << std::endl;
    auto headers = request.getSortedHeaders();
    auto size = std::stoi(headers["Content-Length"]);
    connection->readBody(size, &body_handler);
}

BOOST_AUTO_TEST_CASE(listener)
{
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= boost::log::trivial::warning
    );

    for (int i = 0; i < BODY_SIZE; ++i)
    {
        BODY += char(i % 127);
    }

    HttpServer server;
    server.start();
    server.setSink(&handler);
    server.bind("localhost");

    using boost::asio::ip::tcp;
    boost::asio::io_service io_service;
    tcp::socket s(io_service);
    tcp::resolver resolver(io_service);
    boost::asio::connect(s, resolver.resolve({ "localhost", "8000" }));
    boost::asio::write(s, boost::asio::buffer(REQUEST));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    boost::asio::write(s, boost::asio::buffer(BODY));

    boost::asio::streambuf b;
    boost::asio::read_until(s, b, "\r\n");
    std::istream is(&b);
    std::string line;
    std::getline(is, line);
    boost::trim(line);
    std::cout << line << std::endl;

    BOOST_CHECK_EQUAL(line, "HTTP/1.1 200 Ok");

}

