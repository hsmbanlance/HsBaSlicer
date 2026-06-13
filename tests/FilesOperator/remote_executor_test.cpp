#define BOOST_TEST_MODULE remote_executor_test
#include <boost/test/included/unit_test.hpp>


#include "fileoperator/RemoteExecutor.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(remote_executor_pool_invalid_size)
{
    BOOST_CHECK_THROW(RemoteExecutorConnectionPool("127.0.0.1", "9000", 0), HsBa::Slicer::InvalidArgumentError);
}

BOOST_AUTO_TEST_CASE(remote_executor_connection_invalid_file)
{
    RemoteExecutorConnectionPool pool("127.0.0.1", "9000", 1);
    const std::filesystem::path invalidPath = std::filesystem::temp_directory_path() / "does_not_exist_12345.txt";
    BOOST_CHECK_THROW(pool.SendFile(invalidPath), HsBa::Slicer::IOError);
}
