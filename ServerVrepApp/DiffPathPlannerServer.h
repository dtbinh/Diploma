#pragma once

#include "simpleInConnection.h"
#include "boost\asio\ip\tcp.hpp"
#include <deque>
#include <iostream>
#include <fstream>
#include "Common.h"

using boost::asio::ip::tcp;
using namespace std;

void ParsePathFollowPars(deque<float> &parsIn, PathFollowParamsTypedef &parsOut);
void ParsePathPlannerPars(deque<float> &parsIn, PathPlannerParamsTypedef &parsOut);
void ForwardPathPlannerPars(tcp::iostream &client, PathPlannerParamsTypedef &pars);
int PathPlannerServer(deque<float> &pars, CSimpleInConnection &connection, tcp::iostream &client, ofstream &logFile);