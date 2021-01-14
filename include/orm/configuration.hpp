#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#ifdef TINYORM_COMMON_NAMESPACE
namespace TINYORM_COMMON_NAMESPACE
{
#endif
namespace Orm
{

    using ConfigurationsType = QHash<QString, QVariantHash>;

    struct Configuration
    {
        /*! Default Database Connection Name. */
        QString defaultConnection;

        /*! Database Connections. */
        ConfigurationsType connections;
    };

} // namespace Orm
#ifdef TINYORM_COMMON_NAMESPACE
} // namespace TINYORM_COMMON_NAMESPACE
#endif

#endif // CONFIGURATION_H
