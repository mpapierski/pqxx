/*-------------------------------------------------------------------------
 *
 *   FILE
 *	connection.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::Connection and pqxx::LazyConnection classes.
 *   Different ways of setting up a backend connection.
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */

#include "pqxx/connection.h"

pqxx::Connection::Connection(const PGSTD::string &ConnInfo) :
  ConnectionItf(ConnInfo)
{
  Connect();
}

pqxx::Connection::Connection(const char ConnInfo[]) :
  ConnectionItf(ConnInfo)
{
  Connect();
}

