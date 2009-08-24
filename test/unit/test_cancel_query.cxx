#include <pqxx/compiler-internal.hxx>

#include <test_helpers.hxx>

using namespace PGSTD;
using namespace pqxx;

namespace
{
void test_cancel_query(connection_base &c, transaction_base &t)
{
  pipeline p(t, "test_cancel_query");
  p.retain(0);
  const pipeline::query_id i = p.insert("SELECT pg_sleep(3)");
  c.cancel_query();
#ifdef PQXX_HAVE_PQCANCEL
  PQXX_CHECK_THROWS(p.retrieve(i), sql_error, "Canceled query did not throw.");
#else
   PQXX_CHECK(i, "No query id assigned.");
#endif
}
} // namespace

PQXX_REGISTER_TEST(test_cancel_query)
