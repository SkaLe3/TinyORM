#include "orm/mysqlconnection.hpp"

#include <QtSql/QSqlDriver>

#ifdef TINYORM_MYSQL_PING
#  ifdef __MINGW32__
#    include "mysql/errmsg.h"
#  endif
#  ifdef _MSC_VER
#    include "mysql.h"
#  elif defined(__GNUG__) || defined(__MINGW32__)
#    include "mysql/mysql.h"
#  endif
#endif

#include "orm/query/grammars/mysqlgrammar.hpp"
#include "orm/query/processors/mysqlprocessor.hpp"
#include "orm/schema/grammars/mysqlschemagrammar.hpp"
#include "orm/schema/mysqlschemabuilder.hpp"

#ifdef TINYORM_COMMON_NAMESPACE
namespace TINYORM_COMMON_NAMESPACE
{
#endif
namespace Orm
{

MySqlConnection::MySqlConnection(
        const std::function<Connectors::ConnectionName()> &connection,
        const QString &database, const QString &tablePrefix,
        const QVariantHash &config
)
    : DatabaseConnection(connection, database, tablePrefix, config)
{
    /* We need to initialize a query grammar that is a very important part
       of the database abstraction, so we initialize it to the default value
       while starting. */
    useDefaultQueryGrammar();

    useDefaultPostProcessor();
}

std::unique_ptr<SchemaBuilder> MySqlConnection::getSchemaBuilder()
{
    if (!m_schemaGrammar)
        useDefaultSchemaGrammar();

    return std::make_unique<Schema::MySqlSchemaBuilder>(*this);
}

bool MySqlConnection::isMaria()
{
    // TEST now add MariaDB tests, install mariadb add connection and run all the tests against mariadb too silverqx
    if (!m_isMaria)
        m_isMaria = selectOne("select version()").value(0).value<QString>()
                    .contains("MariaDB");

    return *m_isMaria;
}

bool MySqlConnection::pingDatabase()
{
#ifdef TINYORM_MYSQL_PING
    auto qtConnection = getQtConnection();

    const auto getMysqlHandle = [&qtConnection]() -> MYSQL *
    {
        if (auto driverHandle = qtConnection.driver()->handle();
            qstrcmp(driverHandle.typeName(), "MYSQL*") == 0
        )
            return *static_cast<MYSQL **>(driverHandle.data());

        return nullptr;
    };

    const auto mysqlPing = [getMysqlHandle]() -> bool
    {
        auto mysqlHandle = getMysqlHandle();
        if (mysqlHandle == nullptr)
            return false;

        const auto ping = mysql_ping(mysqlHandle);
        const auto errNo = mysql_errno(mysqlHandle);

        /* So strange logic, because I want interpret CR_COMMANDS_OUT_OF_SYNC errno as
           successful ping. */
        if (ping != 0 && errNo == CR_COMMANDS_OUT_OF_SYNC) {
            // TODO log to file, how often this happen silverqx
            qWarning("mysql_ping() returned : CR_COMMANDS_OUT_OF_SYNC(%ud)", errNo);
            return true;
        }
        else if (ping == 0)
            return true;
        else if (ping != 0)
            return false;
        else {
            qWarning() << "Unknown behavior during mysql_ping(), this should never "
                          "happen :/";
            return false;
        }
    };

    if (qtConnection.isOpen() && mysqlPing()) {
        logConnected();
        return true;
    }

    // The database connection was lost
    logDisconnected();

    // Database connection have to be closed manually
    // isOpen() check is called in MySQL driver
    qtConnection.close();

    // Reset in transaction state and the savepoints counter
    resetTransactions();

    return false;
#else
    throw Exceptions::RuntimeError(
                QStringLiteral(
                    "pingDatabase() method was disabled for the '%1' database driver, "
                    "if you want to use MySqlConnection::pingDatabase(), then "
                    "reconfigure the TinyORM project with the MYSQL_PING preprocessor "
                    "macro ( -DMYSQL_PING ) for cmake or with the 'mysql_ping' "
                    "configuration option ( \"CONFIG+=mysql_ping\" ) for qmake.")
                .arg(driverName()));
#endif
}

std::unique_ptr<QueryGrammar> MySqlConnection::getDefaultQueryGrammar() const
{
    // Ownership of a unique_ptr()
    auto grammar = std::make_unique<Query::Grammars::MySqlGrammar>();

    withTablePrefix(*grammar);

    return grammar;
}

std::unique_ptr<SchemaGrammar> MySqlConnection::getDefaultSchemaGrammar() const
{
    // Ownership of a unique_ptr()
    auto grammar = std::make_unique<Schema::Grammars::MySqlSchemaGrammar>();

    withTablePrefix(*grammar);

    return grammar;
}

std::unique_ptr<QueryProcessor> MySqlConnection::getDefaultPostProcessor() const
{
    return std::make_unique<Query::Processors::MySqlProcessor>();
}

} // namespace Orm
#ifdef TINYORM_COMMON_NAMESPACE
} // namespace TINYORM_COMMON_NAMESPACE
#endif
