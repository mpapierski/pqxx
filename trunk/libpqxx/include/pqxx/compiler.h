/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/compiler.h
 *
 *   DESCRIPTION
 *      Compiler deficiency workarounds
 *
 * Copyright (c) 2002, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_COMPILER_H
#define PQXX_COMPILER_H

#include "pqxx/config.h"

#ifdef BROKEN_ITERATOR
namespace PGSTD
{
/// Deal with lacking iterator template definition in <iterator>
template<typename Cat, 
         typename T, 
	 typename Dist, 
	 typename Ptr=T*,
	 typename Ref=T&> struct iterator
{
  typedef Cat iterator_category;
  typedef T value_type;
  typedef Dist difference_type;
  typedef Ptr pointer;
  typedef Ref reference;
};
}
#else
#include <iterator>
#endif // BROKEN_ITERATOR


#ifdef HAVE_LIMITS
#include <limits>
#else // HAVE_LIMITS
#include <climits>
namespace PGSTD
{
/// Deal with lacking <limits>
template<typename T> struct numeric_limits
{
  static T max() throw ();
  static T min() throw ();
};

template<> inline long numeric_limits<long>::max() throw () {return LONG_MAX;}
template<> inline long numeric_limits<long>::min() throw () {return LONG_MIN;}
}
#endif // HAVE_LIMITS


// Used for Windows DLL
#ifndef PQXX_LIBEXPORT
#define PQXX_LIBEXPORT
#endif

#endif

