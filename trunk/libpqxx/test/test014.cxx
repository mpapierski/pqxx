#include <iostream>

#include <pqxx/connection.h>
#include <pqxx/nontransaction.h>
#include <pqxx/result.h>

using namespace PGSTD;
using namespace pqxx;


// Define a pqxx::Noticer to process warnings generated by the database 
// connection and (in this case) pass them to cerr.  This is optional.
namespace
{
struct ReportWarning : Noticer
{
  virtual void operator()(const char Msg[]) throw ()
  {
    cerr << Msg;
  }
};
}


// Simple test program for libpqxx.  Open connection to database, start
// a dummy transaction to gain nontransactional access, and perform a query.
//
// Usage: test14 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a
// backend running on host foo.bar.net, logging in as user smith.
int main(int, char *argv[])
{
  try
  {
    Connection C(argv[1]);

    // Tell C to report its warnings through std::cerr instead of the default
    // (which is to print to stderr).  This is done just to show that the way
    // messages are processed can be changed by the client program.
    C.SetNoticer(auto_ptr<Noticer>(new ReportWarning));

    // Now use our Noticer to output a diagnostic message.  Note that the
    // message must end in a newline.
    C.ProcessNotice("Opened connection\n");

    // ProcessNotice() can take either a C++ string or a C-style const char *.
    const string HostName = (C.HostName() ? C.HostName() : "<local>");
    C.ProcessNotice(string() +
		    "database=" + C.DbName() + ", "
		    "username=" + C.UserName() + ", "
		    "hostname=" + HostName + ", "
		    "port=" + ToString(C.Port()) + ", "
		    "options='" + C.Options() + "', "
		    "backendpid=" + ToString(C.BackendPID()) + "\n");

    // Begin a "non-transaction" acting on our current connection.  This is
    // really all the transactional integrity we need since we're only 
    // performing one query which does not modify the database.
    NonTransaction T(C, "test14");

    // The Transaction family of classes also has ProcessNotice() functions.
    // These simply pass the notice through to their connection, but this may
    // be more convenient in some cases.  All ProcessNotice() functions accept
    // C++ strings as well as C strings.
    T.ProcessNotice(string("Started nontransaction\n"));

    Result R( T.Exec("SELECT * FROM pg_tables") );

    // Give some feedback to the test program's user prior to the real work
    T.ProcessNotice(ToString(R.size()) + " "
		    "result tuples in transaction " +
		    T.Name() +
		    "\n");

    for (Result::const_iterator c = R.begin(); c != R.end(); ++c)
    {
      string N;
      c[0].to(N);

      cout << '\t' << ToString(c.num()) << '\t' << N << endl;
    }

    // "Commit" the non-transaction.  This doesn't really do anything since
    // NonTransaction doesn't start a backend transaction.
    T.Commit();
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

