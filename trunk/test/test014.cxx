#include <cassert>
#include <iostream>

#include <pqxx/connection>
#include <pqxx/nontransaction>
#include <pqxx/result>

using namespace PGSTD;
using namespace pqxx;


// Define a pqxx::noticer to process warnings generated by the database
// connection and (in this case) pass them to cerr.  This is optional.
namespace
{
struct ReportWarning : noticer
{
  ReportWarning():noticer(){}	// Silences bogus warning in some gcc versions

  virtual void operator()(const char Msg[]) throw ()
  {
    cerr << Msg;
  }
};
}


// Simple test program for libpqxx.  Open connection to database, start
// a dummy transaction to gain nontransactional access, and perform a query.
//
// Usage: test014 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a
// backend running on host foo.bar.net, logging in as user smith.
int main(int, char *argv[])
{
  try
  {
    connection C(argv[1]);

    // Tell C to report its warnings through std::cerr instead of the default
    // (which is to print to stderr).  This is done just to show that the way
    // messages are processed can be changed by the client program.
    noticer *MyNoticer = new ReportWarning;
    // This is not a memory leak: C stores MyNoticer in an auto_ptr that will
    // delete the object on destruction.
    C.set_noticer(auto_ptr<noticer>(MyNoticer));

    assert(C.get_noticer() == MyNoticer);

    // Now use our noticer to output a diagnostic message.  Note that the
    // message must end in a newline.
    C.process_notice("Opened connection\n");

    // ProcessNotice() can take either a C++ string or a C-style const char *.
    const string HostName = (C.hostname() ? C.hostname() : "<local>");
    C.process_notice(string() +
		     "database=" + C.dbname() + ", "
		     "username=" + C.username() + ", "
		     "hostname=" + HostName + ", "
		     "port=" + to_string(C.port()) + ", "
		     "options='" + C.options() + "', "
		     "backendpid=" + to_string(C.backendpid()) + "\n");

    // Begin a "non-transaction" acting on our current connection.  This is
    // really all the transactional integrity we need since we're only
    // performing one query which does not modify the database.
    nontransaction T(C, "test14");

    // The Transaction family of classes also has ProcessNotice() functions.
    // These simply pass the notice through to their connection, but this may
    // be more convenient in some cases.  All ProcessNotice() functions accept
    // C++ strings as well as C strings.
    T.process_notice(string("Started nontransaction\n"));

    result R( T.exec("SELECT * FROM pg_tables") );

    // Give some feedback to the test program's user prior to the real work
    T.process_notice(to_string(R.size()) + " "
		    "result tuples in transaction " +
		    T.name() +
		    "\n");

    for (result::const_iterator c = R.begin(); c != R.end(); ++c)
    {
      string N;
      c[0].to(N);

      cout << '\t' << to_string(c.num()) << '\t' << N << endl;
    }

    // "Commit" the non-transaction.  This doesn't really do anything since
    // NonTransaction doesn't start a backend transaction.
    T.commit();
  }
  catch (const sql_error &e)
  {
    // If we're interested in the text of a failed query, we can write separate
    // exception handling code for this type of exception
    cerr << "SQL error: " << e.what() << endl
         << "Query was: '" << e.query() << "'" << endl;
    return 1;
  }
  catch (const exception &e)
  {
    // All exceptions thrown by libpqxx are derived from std::exception
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }
  catch (...)
  {
    // This is really unexpected (see above)
    cerr << "Unhandled exception" << endl;
    return 100;
  }

  return 0;
}

