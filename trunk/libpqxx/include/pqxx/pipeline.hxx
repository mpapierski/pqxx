/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/pipeline.hxx
 *
 *   DESCRIPTION
 *      definition of the pqxx::pipeline class.
 *   Throughput-optimized query manager
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/pipeline instead.
 *
 * Copyright (c) 2003-2004, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler.h"

#include <map>
#include <string>
#include <vector>

#include "pqxx/transaction_base"


/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */

namespace pqxx
{

// TODO: Warning for unretrieved results
// TODO: "sessionfocus" class (common base with tablestream) to block xaction


/// Processes several queries in FIFO manner, optimized for high throughput
/** @warning This is an early prototype, not functional code, and it will most
 * likely change radically before becoming part of the library.
 *
 * Use a pipeline if you want to execute several queries in succession, in
 * situations where some of the queries do not depend on the outcome of the
 * preceding one.  Result retrieval is decoupled from execution request; queries
 * "go in at the front" and results "come out the back." In fact, results may be
 * retrieved in any order--but this will typically be slower than sticking to
 * the order in which they were entered.
 *
 * Feel free to pump as many queries into the pipeline as possible, even if they
 * were generated after looking at a result from the same pipeline.  To get the
 * best possible throughput, try to make insertion of queries run as far ahead
 * of results retrieval as possible; issue each query as early as possible and
 * retrieve their results as late as possible, so the pipeline has as many
 * ongoing queries as possible at any given time.  In other words, keep it busy!
 */
class PQXX_LIBEXPORT pipeline
{
public:
  typedef unsigned query_id;

  explicit pipeline(transaction_base &t);				//[t69]
  ~pipeline();

  /// Add query to the pipeline.
  query_id insert(const PGSTD::string &);				//[t69]

  /// Wait for all ongoing or pending operations to complete
  void complete();							//[t71]

  /// Forget all pending operations and retrieved results
  void flush();								//[t70]

  /// Has given query started yet?
  bool is_running(query_id) const;					//[t71]

  /// Is result for given query available?
  bool is_finished(query_id) const;					//[t71]

  /// Retrieve result for given query
  /** If the query failed for whatever reason, this will throw an exception.
   * The function will block if the query has not finished yet.
   */
  result retrieve(query_id);						//[t71]

  /// Retrieve oldest unretrieved result (possibly wait for one)
  PGSTD::pair<query_id, result> retrieve();				//[t69]

  bool empty() const throw ();						//[t69]

  /// Optimization control: don't start issuing yet, more queries are coming
  /** The pipeline will normally issue the first query inserted into it to the
   * backend, and accumulate any new queries inserted while it is executing; 
   * those will once again be issued at the earliest opportunity.  This may not
   * be optimal, since most of the pipeline's speed advantage comes from its 
   * ability to bundle queries together and issue them at once.  The normal
   * procedure will be too "eager" to provide the full benefit for that first
   * query.
   * Call retain() to tell the pipeline to hold off on issuing queries when you
   * know that more queries are about to be inserted.  Use resume() afterwards,
   * or the queries may not be issued for some time.  Query emission will be 
   * resumed implicitly, however, if results are requested for queries that have
   * not yet been issued to the backend.
   * For best performance, call retain() before inserting a batch of queries 
   * into a pipeline, and resume() directly afterwards to make sure the queries
   * are sent to the backend.
   */
  void retain() 				{ m_retain = true; }	//[t70]

  /// Resume retained query emission (harmless when not needed)
  void resume();							//[t70]

private:
  /// Create new query_id
  query_id generate_id();
  /// Attach to transaction
  void attach();
  /// Detach from transaction
  void detach();
  /// Gather up waiting queries & send them
  void send_waiting();
  /// Accept any received result sets and keep them until requested
  void consumeresults();
  /// Check result for errors, remove it from internal state, and return it
  PGSTD::pair<query_id, result> deliver(PGSTD::map<query_id, result>::iterator);

  transaction_base &m_home;
  PGSTD::map<query_id, PGSTD::string> m_queries;
  PGSTD::vector<query_id> m_waiting, m_sent;
  PGSTD::map<query_id, result> m_completed;
  query_id m_nextid;
  bool m_retain;

  /// Not allowed
  pipeline(const pipeline &);
  /// Not allowed
  pipeline &operator=(const pipeline &);
};


} // namespace


