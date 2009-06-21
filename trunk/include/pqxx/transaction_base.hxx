/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/transaction_base.hxx
 *
 *   DESCRIPTION
 *      common code and definitions for the transaction classes.
 *   pqxx::transaction_base defines the interface for any abstract class that
 *   represents a database transaction
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/transaction_base instead.
 *
 * Copyright (c) 2001-2009, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_H_TRANSACTION_BASE
#define PQXX_H_TRANSACTION_BASE

#include "pqxx/compiler-public.hxx"
#include "pqxx/compiler-internal-pre.hxx"

/* End-user programs need not include this file, unless they define their own
 * transaction classes.  This is not something the typical program should want
 * to do.
 *
 * However, reading this file is worthwhile because it defines the public
 * interface for the available transaction classes such as transaction and
 * nontransaction.
 */

#include "pqxx/connection_base"
#include "pqxx/isolation"
#include "pqxx/result"

/* Methods tested in eg. self-test program test001 are marked with "//[t1]"
 */

namespace pqxx
{
class connection_base;
class transaction_base;


namespace internal
{
class sql_cursor;

class PQXX_LIBEXPORT transactionfocus : public virtual namedclass
{
public:
  explicit transactionfocus(transaction_base &t) :
    namedclass("transactionfocus"),
    m_Trans(t),
    m_registered(false)
  {
  }

protected:
  void register_me();
  void unregister_me() throw ();
  void reg_pending_error(const PGSTD::string &) throw ();
  bool registered() const throw () { return m_registered; }

  transaction_base &m_Trans;

private:
  bool m_registered;

  /// Not allowed
  transactionfocus();
  /// Not allowed
  transactionfocus(const transactionfocus &);
  /// Not allowed
  transactionfocus &operator=(const transactionfocus &);
};

} // namespace internal


namespace internal
{
class transaction_subtransaction_gate;
class transaction_tablereader_gate;
class transaction_tablewriter_gate;
class transaction_transactionfocus_gate;
} // namespace internal


/// Interface definition (and common code) for "transaction" classes.
/**
 * @addtogroup transaction Transaction classes
 * All database access must be channeled through one of these classes for
 * safety, although not all implementations of this interface need to provide
 * full transactional integrity.
 *
 * Several implementations of this interface are shipped with libpqxx, including
 * the plain transaction class, the entirely unprotected nontransaction, and the
 * more cautions robusttransaction.
 */
class PQXX_LIBEXPORT PQXX_NOVTABLE transaction_base :
  public virtual internal::namedclass
{
public:
  /// If nothing else is known, our isolation level is at least read_committed
  typedef isolation_traits<read_committed> isolation_tag;

  virtual ~transaction_base() =0;					//[t1]

  /// Commit the transaction
  /** Unless this function is called explicitly, the transaction will not be
   * committed (actually the nontransaction implementation breaks this rule,
   * hence the name).
   *
   * Once this function returns, the whole transaction will typically be
   * irrevocably completed in the database.  There is also, however, a minute
   * risk that the connection to the database may be lost at just the wrong
   * moment.  In that case, libpqxx may be unable to determine whether the
   * transaction was completed or aborted and an in_doubt_error will be thrown
   * to make this fact known to the caller.  The robusttransaction
   * implementation takes some special precautions to reduce this risk.
   */
  void commit();							//[t1]

  /// Abort the transaction
  /** No special effort is required to call this function; it will be called
   * implicitly when the transaction is destructed.
   */
  void abort();								//[t10]

  /**
   * @addtogroup escaping String escaping
   */
  //@{
  /// Escape string for use as SQL string literal in this transaction
  PGSTD::string esc(const char str[]) const				//[t90]
                                                     { return m_Conn.esc(str); }
  /// Escape string for use as SQL string literal in this transaction
  PGSTD::string esc(const char str[], size_t maxlen) const		//[t90]
                                             { return m_Conn.esc(str, maxlen); }
  /// Escape string for use as SQL string literal in this transaction
  PGSTD::string esc(const PGSTD::string &str) const			//[t90]
                                                     { return m_Conn.esc(str); }

  /// Escape binary data for use as SQL string literal in this transaction
  /** Raw, binary data is treated differently from regular strings.  Binary
   * strings are never interpreted as text, so they may safely include byte
   * values or byte sequences that don't happen to represent valid characters in
   * the character encoding being used.
   *
   * The binary string does not stop at the first zero byte, as is the case with
   * textual strings.  Instead, they may contain zero bytes anywhere.  If it
   * happens to contain bytes that look like quote characters, or other things
   * that can disrupt their use in SQL queries, they will be replaced with
   * special escape sequences.
   */
  PGSTD::string esc_raw(const unsigned char str[], size_t len) const	//[t62]
                                            { return m_Conn.esc_raw(str, len); }
  /// Escape binary data for use as SQL string literal in this transaction
  PGSTD::string esc_raw(const PGSTD::string &) const;			//[t62]

  /// Represent object as SQL string, including quoting & escaping.
  /** Nulls are recognized and represented as SQL nulls. */
  template<typename T> PGSTD::string quote(const T &t) const
                                                     { return m_Conn.quote(t); }
  //@}

  /// Execute query
  /** Perform a query in this transaction.
   *
   * This is one of the most important functions in libpqxx.
   *
   * Most libpqxx exceptions can be thrown from here, including sql_error,
   * broken_connection, and many sql_error subtypes such as
   * feature_not_supported or insufficient_privilege.  But any exception thrown
   * by the C++ standard library may also occur here.  All exceptions will be
   * derived from std::exception, however, and all libpqxx-specific exception
   * types are derived from pqxx::pqxx_exception.
   *
   * @param Query Query or command to execute
   * @param Desc Optional identifier for query, to help pinpoint SQL errors
   * @return A result set describing the query's or command's result
   */
  result exec(const PGSTD::string &Query,
	      const PGSTD::string &Desc=PGSTD::string());		//[t1]

  result exec(const PGSTD::stringstream &Query,
	      const PGSTD::string &Desc=PGSTD::string())		//[t9]
	{ return exec(Query.str(), Desc); }

  /**
   * @name Prepared statements
   */
  //@{
  /// Execute prepared statement.
  /** Prepared statements are defined using the connection classes' prepare()
   * function, and continue to live on in the ongoing session regardless of
   * the context they were defined in (unless explicitly dropped using the
   * connection's unprepare() function).  Their execution however, like other
   * forms of query execution, requires a transaction object.
   *
   * Just like param_declaration is a helper class that lets you tag parameter
   * declarations onto the statement declaration, the invocation class returned
   * here lets you tag parameter values onto the call:
   *
   * @code
   * result run_mystatement(transaction_base &T)
   * {
   *   return T.prepared("mystatement")("param1")(2)()(4).exec();
   * }
   * @endcode
   *
   * Here, parameter 1 (written as "<tt>$1</tt>" in the statement's body) is a
   * string that receives the value "param1"; the second parameter is an integer
   * with the value 2; the third receives a null, making its type irrelevant;
   * and number 4 again is an integer.  The ultimate invocation of exec() is
   * essential; if you forget this, nothing happens.
   *
   * To see whether any prepared statement has been defined under a given name,
   * use:
   *
   * @code
   * T.prepared("mystatement").exists()
   * @endcode
   *
   * @warning Do not try to execute a prepared statement manually through direct
   * SQL statements.  This is likely not to work, and even if it does, is likely
   * to be slower than using the proper libpqxx functions.  Also, libpqxx knows
   * how to emulate prepared statements if some part of the infrastructure does
   * not support them.
   *
   * @warning Actual definition of the prepared statement on the backend may be
   * deferred until its first use, which means that any errors in the prepared
   * statement may not show up until it is executed--and perhaps abort the
   * ongoing transaction in the process.
   *
   * If you leave out the statement name, the call refers to the nameless
   * statement instead.
   */
  prepare::invocation prepared(const PGSTD::string &statement=PGSTD::string());

  //@}

  /**
   * @name Error/warning output
   */
  //@{
  /// Have connection process warning message
  void process_notice(const char Msg[]) const				//[t14]
	{ m_Conn.process_notice(Msg); }
  /// Have connection process warning message
  void process_notice(const PGSTD::string &Msg) const			//[t14]
	{ m_Conn.process_notice(Msg); }
  //@}

  /// Connection this transaction is running in
  connection_base &conn() const { return m_Conn; }			//[t4]

  /// Set session variable in this connection
  /** The new value is typically forgotten if the transaction aborts.
   * Known exceptions to this rule are nontransaction, and PostgreSQL versions
   * prior to 7.3.  In the case of nontransaction, the set value will be kept
   * regardless; but in that case, if the connection ever needs to be recovered,
   * the set value will not be restored.
   * @param Var The variable to set
   * @param Val The new value to store in the variable
   */
  void set_variable(const PGSTD::string &Var, const PGSTD::string &Val);//[t61]

  /// Get currently applicable value of variable
  /** First consults an internal cache of variables that have been set (whether
   * in the ongoing transaction or in the connection) using the set_variable
   * functions.  If it is not found there, the database is queried.
   *
   * @warning Do not mix the set_variable with raw "SET" queries, and do not
   * try to set or get variables while a pipeline or table stream is active.
   *
   * @warning This function used to be declared as @c const but isn't anymore.
   */
  PGSTD::string get_variable(const PGSTD::string &);			//[t61]


protected:
  /// Create a transaction (to be called by implementation classes only)
  /** The optional name, if nonempty, must begin with a letter and may contain
   * letters and digits only.
   *
   * @param c The connection that this transaction is to act on.
   * @param direct Running directly in connection context (i.e. not nested)?
   */
  explicit transaction_base(connection_base &c, bool direct=true);

  /// Begin transaction (to be called by implementing class)
  /** Will typically be called from implementing class' constructor.
   */
  void Begin();

  /// End transaction.  To be called by implementing class' destructor
  void End() throw ();

  /// To be implemented by derived implementation class: start transaction
  virtual void do_begin() =0;
  /// To be implemented by derived implementation class: perform query
  virtual result do_exec(const char Query[]) =0;
  /// To be implemented by derived implementation class: commit transaction
  virtual void do_commit() =0;
  /// To be implemented by derived implementation class: abort transaction
  virtual void do_abort() =0;

  // For use by implementing class:

  /// Execute query on connection directly
  /**
   * @param C Query or command to execute
   * @param Retries Number of times to retry the query if it fails.  Be
   * extremely careful with this option; if you retry in the middle of a
   * transaction, you may be setting up a new connection transparently and
   * executing the latter part of the transaction without a backend transaction
   * being active (and with the former part aborted).
   */
  result DirectExec(const char C[], int Retries=0);

  /// Forget about any reactivation-blocking resources we tried to allocate
  void reactivation_avoidance_clear() throw ()
	{m_reactivation_avoidance.clear();}

protected:
  /// Resources allocated in this transaction that make reactivation impossible
  /** This number may be negative!
   */
  internal::reactivation_avoidance_counter m_reactivation_avoidance;

private:
  /* A transaction goes through the following stages in its lifecycle:
   * <ul>
   * <li> nascent: the transaction hasn't actually begun yet.  If our connection
   *    fails at this stage, it may recover and the transaction can attempt to
   *    establish itself again.
   * <li> active: the transaction has begun.  Since no commit command has been
   *    issued, abortion is implicit if the connection fails now.
   * <li> aborted: an abort has been issued; the transaction is terminated and
   *    its changes to the database rolled back.  It will accept no further
   *    commands.
   * <li> committed: the transaction has completed successfully, meaning that a
   *    commit has been issued.  No further commands are accepted.
   * <li> in_doubt: the connection was lost at the exact wrong time, and there
   *    is no way of telling whether the transaction was committed or aborted.
   * </ul>
   *
   * Checking and maintaining state machine logic is the responsibility of the
   * base class (ie., this one).
   */
  enum Status
  {
    st_nascent,
    st_active,
    st_aborted,
    st_committed,
    st_in_doubt
  };

  /// Make sure transaction is opened on backend, if appropriate
  void PQXX_PRIVATE activate();

  void PQXX_PRIVATE CheckPendingError();

  template<typename T> bool parm_is_null(T *p) const throw () { return !p; }
  template<typename T> bool parm_is_null(T) const throw () { return false; }

  friend class pqxx::internal::transaction_transactionfocus_gate;
  void PQXX_PRIVATE RegisterFocus(internal::transactionfocus *);
  void PQXX_PRIVATE UnregisterFocus(internal::transactionfocus *) throw ();
  void PQXX_PRIVATE RegisterPendingError(const PGSTD::string &) throw ();

  friend class pqxx::internal::transaction_tablereader_gate;
  void PQXX_PRIVATE BeginCopyRead(const PGSTD::string &, const PGSTD::string &);
  bool ReadCopyLine(PGSTD::string &);

  friend class pqxx::internal::transaction_tablewriter_gate;
  void PQXX_PRIVATE BeginCopyWrite(
	const PGSTD::string &Table,
	const PGSTD::string &Columns);
  void WriteCopyLine(const PGSTD::string &);
  void EndCopyWrite();

  friend class pqxx::internal::transaction_subtransaction_gate;

  connection_base &m_Conn;

  internal::unique<internal::transactionfocus> m_Focus;
  Status m_Status;
  bool m_Registered;
  PGSTD::map<PGSTD::string, PGSTD::string> m_Vars;
  PGSTD::string m_PendingError;

  /// Not allowed
  transaction_base();
  /// Not allowed
  transaction_base(const transaction_base &);
  /// Not allowed
  transaction_base &operator=(const transaction_base &);
};

} // namespace pqxx


#include "pqxx/compiler-internal-post.hxx"

#endif

