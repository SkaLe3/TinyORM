#ifndef POSTGRESCONNECTION_HPP
#define POSTGRESCONNECTION_HPP

#include "orm/databaseconnection.hpp"

#ifdef TINYORM_COMMON_NAMESPACE
namespace TINYORM_COMMON_NAMESPACE
{
#endif
namespace Orm
{

    class SHAREDLIB_EXPORT PostgresConnection final : public DatabaseConnection
    {
        Q_DISABLE_COPY(PostgresConnection)

    public:
        PostgresConnection(
                const std::function<Connectors::ConnectionName()> &connection,
                const QString &database = "", const QString &tablePrefix = "",
                const QVariantHash &config = {});
        inline virtual ~PostgresConnection() = default;

        /*! Get a schema builder instance for the connection. */
        std::unique_ptr<SchemaBuilder> getSchemaBuilder() override;

    protected:
        /*! Get the default query grammar instance. */
        std::unique_ptr<QueryGrammar> getDefaultQueryGrammar() const override;
        /*! Get the default schema grammar instance. */
        std::unique_ptr<SchemaGrammar> getDefaultSchemaGrammar() const override;
        /*! Get the default post processor instance. */
        std::unique_ptr<QueryProcessor> getDefaultPostProcessor() const override;
    };

} // namespace Orm
#ifdef TINYORM_COMMON_NAMESPACE
} // namespace TINYORM_COMMON_NAMESPACE
#endif

#endif // POSTGRESCONNECTION_HPP
