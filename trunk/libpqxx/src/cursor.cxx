/*-------------------------------------------------------------------------
 *
 *   FILE
 *	cursor.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::Cursor class.
 *   pqxx::Cursor represents a database cursor.
 *
 * Copyright (c) 2001-2002, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/cursor.h"
#include "pqxx/result.h"
#include "pqxx/transaction.h"

using namespace PGSTD;


pqxx::Cursor::Cursor(pqxx::TransactionItf &T, 
		   const char Query[],
		   string BaseName,
		   pqxx::Result_size_type Count) :
  m_Trans(T),
  m_Name(BaseName),
  m_Count(Count),
  m_Done(false)
{
  // Give ourselves a locally unique name based on connection name
  m_Name += "_" + T.Name() + "_" + ToString(T.GetUniqueCursorNum());

  m_Trans.Exec(("DECLARE " + m_Name + " CURSOR FOR " + Query).c_str());
}


pqxx::Result_size_type pqxx::Cursor::SetCount(pqxx::Result_size_type Count)
{
  Result_size_type Old = m_Count;
  m_Done = false;
  m_Count = Count;
  return Old;
}


pqxx::Cursor &pqxx::Cursor::operator>>(pqxx::Result &R)
{
  R = Fetch(m_Count);
  m_Done = R.empty();
  return *this;
}


pqxx::Result pqxx::Cursor::Fetch(pqxx::Result_size_type Count)
{
  pqxx::Result R;

  if (Count == 0) 
  {
    m_Trans.MakeEmpty(R);
    return R;
  }

  R = m_Trans.Exec(MakeFetchCmd(Count).c_str());
  m_Done = false;
  return R;
}


void pqxx::Cursor::Move(pqxx::Result_size_type Count)
{
  if (Count == 0) return;

  m_Done = false;

  m_Trans.Exec(("MOVE " + OffsetString(Count) + " IN " + m_Name).c_str());
}


string pqxx::Cursor::OffsetString(Result_size_type Count)
{
  if (Count == ALL()) return "ALL";
  else if (Count == BACKWARD_ALL()) return "BACKWARD ALL";

  return ToString(Count);
}


string pqxx::Cursor::MakeFetchCmd(pqxx::Result_size_type Count) const
{
  if (!Count) throw logic_error("Internal libpqxx error: Cursor: zero count");
  return "FETCH " + OffsetString(Count) + " IN " + m_Name;
}

