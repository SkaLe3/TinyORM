#include "orm/query/joinclause.hpp"

#ifdef TINYORM_COMMON_NAMESPACE
namespace TINYORM_COMMON_NAMESPACE
{
#endif
namespace Orm::Query
{

// TODO check newQuery(), forSubQuery(), newParentQuery() in JoinClause, they have separate implementation in Eloquent silverqx
JoinClause::JoinClause(const Builder &query, const QString &type, const QString &table)
    : Builder(query.getConnection(), query.getGrammar())
    , m_type(type)
    , m_table(table)
{}

JoinClause &JoinClause::on(const QString &first, const QString &comparison,
                           const QString &second, const QString &condition)
{
    /* On clauses can be chained, e.g.

       $join->on('contacts.user_id', '=', 'users.id')
            ->on('contacts.info_id', '=', 'info.id')

       will produce the following SQL:

       on `contacts`.`user_id` = `users`.`id` and `contacts`.`info_id` = `info`.`id` */

    whereColumn(first, comparison, second, condition);

    return *this;
}

} // namespace Orm
#ifdef TINYORM_COMMON_NAMESPACE
} // namespace TINYORM_COMMON_NAMESPACE
#endif
