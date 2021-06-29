#include "orm/query/querybuilder.hpp"

#include "orm/databaseconnection.hpp"
#include "orm/query/joinclause.hpp"

#ifdef TINYORM_COMMON_NAMESPACE
namespace TINYORM_COMMON_NAMESPACE
{
#endif
namespace Orm::Query
{

Builder::Builder(ConnectionInterface &connection, const QueryGrammar &grammar)
    : m_connection(connection)
    , m_grammar(grammar)
{}

QSqlQuery
Builder::get(const QStringList &columns)
{
    return onceWithColumns(columns, [this]
    {
        return runSelect();
    });
}

QSqlQuery
Builder::find(const QVariant &id, const QStringList &columns)
{
    return where("id", "=", id).first(columns);
}

QSqlQuery Builder::first(const QStringList &columns)
{
    auto query = take(1).get(columns);

    query.first();

    return query;
}

QVariant Builder::value(const QString &column)
{
    return first({column}).value(column);
}

QString Builder::toSql()
{
    return m_grammar.compileSelect(*this);
}

std::optional<QSqlQuery>
Builder::insert(const QVariantMap &values)
{
    return insert(QVector<QVariantMap> {values});
}

namespace
{
    const auto flatValuesForInsert = [](const auto &values)
    {
        QVector<QVariant> flattenValues;
        for (const auto &insertMap : values)
            for (const auto &value : insertMap)
                flattenValues << value;

        return flattenValues;
    };
}

// TEST for insert silverqx
std::optional<QSqlQuery>
Builder::insert(const QVector<QVariantMap> &values)
{
    if (values.isEmpty())
        return std::nullopt;

    /* The logic described below is guaranteed by QVariantMap, keys are ordered
       by default.
       Here, we will sort the insert keys for every record so that each insert is
       in the same order for the record. We need to make sure this is the case
       so there are not any errors or problems when inserting these records. */

    return m_connection.insert(m_grammar.compileInsert(*this, values),
                               cleanBindings(flatValuesForInsert(values)));
}

// FEATURE dilemma primarykey, add support for Model::KeyType in QueryBuilder/TinyBuilder or should it be QVariant and runtime type check? 🤔 silverqx
quint64 Builder::insertGetId(const QVariantMap &values, const QString &sequence)
{
    const QVector<QVariantMap> valuesVector {values};

    auto query = m_connection.insert(
                     m_grammar.compileInsertGetId(*this, valuesVector, sequence),
                     cleanBindings(flatValuesForInsert(valuesVector)));

    // FEATURE dilemma primarykey, Model::KeyType vs QVariant, Processor::processInsertGetId() silverqx
    return query.lastInsertId().value<quint64>();
}

std::tuple<int, std::optional<QSqlQuery>>
Builder::insertOrIgnore(const QVector<QVariantMap> &values)
{
    if (values.isEmpty())
        return {0, std::nullopt};

    return m_connection.affectingStatement(
                m_grammar.compileInsertOrIgnore(*this, values),
                cleanBindings(flatValuesForInsert(values)));
}

std::tuple<int, std::optional<QSqlQuery>>
Builder::insertOrIgnore(const QVariantMap &values)
{
    return insertOrIgnore(QVector<QVariantMap> {values});
}

std::tuple<int, QSqlQuery>
Builder::update(const QVector<UpdateItem> &values)
{
    return m_connection.update(
                m_grammar.compileUpdate(*this, values),
                cleanBindings(m_grammar.prepareBindingsForUpdate(getRawBindings(),
                                                                 values)));
}

std::tuple<int, QSqlQuery> Builder::deleteRow()
{
    return remove();
}

std::tuple<int, QSqlQuery> Builder::remove()
{
    return m_connection.remove(
            m_grammar.compileDelete(*this),
            cleanBindings(m_grammar.prepareBindingsForDelete(getRawBindings())));
}

std::tuple<int, QSqlQuery> Builder::deleteRow(const quint64 id)
{
    return remove(id);
}

std::tuple<int, QSqlQuery> Builder::remove(const quint64 id)
{
    /* If an ID is passed to the method, we will set the where clause to check the
       ID to let developers to simply and quickly remove a single row from this
       database without manually specifying the "where" clauses on the query.
       m_from will be wrapped in the Grammar. */
    where(QStringLiteral("%1.id").arg(std::get<QString>(m_from)),
          QStringLiteral("="), id, QStringLiteral("and"));

    return remove();
}

void Builder::truncate()
{
    for (const auto &[sql, bindings] : m_grammar.compileTruncate(*this))
        /* Postgres doesn't execute truncate statement as prepared query:
           https://www.postgresql.org/docs/13/sql-prepare.html */
        if (m_connection.driverName() == "QPSQL")
            m_connection.unprepared(sql);
        else
            m_connection.statement(sql, bindings);
}

Builder &Builder::select(const QStringList &columns)
{
    // FEATURE expressions, add Query::Expression overload, find all occurences of Illuminate\Database\Query\Expression in the Eloquent and add support to TinyORM, I will need to add overloads for some methods, for columns and also for values silverqx
    clearColumns();

    std::ranges::copy(columns, std::back_inserter(m_columns));

    return *this;
}

Builder &Builder::select(const QString &column)
{
    return select(QStringList(column));
}

Builder &Builder::addSelect(const QStringList &columns)
{
    std::ranges::copy(columns, std::back_inserter(m_columns));

    return *this;
}

// FUTURE when appropriate, move inline definitions outside class, check all inline to see what to do silverqx
Builder &Builder::addSelect(const QString &column)
{
    return addSelect(QStringList(column));
}

Builder &Builder::distinct()
{
    m_distinct = true;

    return *this;
}

Builder &Builder::distinct(const QStringList &columns)
{
    m_distinct = columns;

    return *this;
}

Builder &Builder::distinct(QStringList &&columns)
{
    m_distinct = std::move(columns);

    return *this;
}

Builder &Builder::from(const QString &table, const QString &as)
{
    m_from = as.isEmpty() ? table : QStringLiteral("%1 as %2").arg(table, as);

    return *this;
}

Builder &Builder::from(const Expression &table)
{
    m_from.emplace<Expression>(table);

    return *this;
}

Builder &Builder::from(Expression &&table)
{
    m_from.emplace<Expression>(std::move(table));

    return *this;
}

Builder &Builder::fromRaw(const QString &expression, const QVector<QVariant> &bindings)
{
    m_from.emplace<Expression>(expression);

    addBinding(bindings, BindingType::FROM);

    return *this;
}

Builder &Builder::where(const QString &column, const QString &comparison,
                        const QVariant &value, const QString &condition)
{
    // Compile check for a invalid comparison operator
    invalidOperator(comparison);

    m_wheres.append({column, value, comparison, condition, WhereType::BASIC});

    addBinding(value, BindingType::WHERE);

    return *this;
}

Builder &Builder::orWhere(const QString &column, const QString &comparison,
                          const QVariant &value)
{
    return where(column, comparison, value, QStringLiteral("or"));
}

Builder &Builder::whereEq(const QString &column, const QVariant &value,
                          const QString &condition)
{
    return where(column, QStringLiteral("="), value, condition);
}

Builder &Builder::orWhereEq(const QString &column, const QVariant &value)
{
    return where(column, QStringLiteral("="), value, QStringLiteral("or"));
}

Builder &Builder::where(const std::function<void(Builder &)> &callback,
                        const QString &condition)
{
    // Ownership of the QSharedPointer<QueryBuilder>
    const auto query = forNestedWhere();

    std::invoke(callback, *query);

    return addNestedWhereQuery(query, condition);
}

Builder &Builder::orWhere(const std::function<void(Builder &)> &callback)
{
    return where(callback, QStringLiteral("or"));
}

Builder &Builder::where(const QVector<WhereItem> &values, const QString &condition)
{
    /* We will maintain the boolean we received when the method was called and pass it
       into the nested where.
       The parentheses in this query are ok:
       select * from xyz where (id = ?) */
    return addArrayOfWheres(values, condition);
}

Builder &Builder::orWhere(const QVector<WhereItem> &values)
{
    return where(values, QStringLiteral("or"));
}

Builder &Builder::whereColumn(const QVector<WhereColumnItem> &values,
                              const QString &condition)
{
    return addArrayOfWheres(values, condition);
}

Builder &Builder::orWhereColumn(const QVector<WhereColumnItem> &values)
{
    return addArrayOfWheres(values, QStringLiteral("or"));
}

Builder &Builder::whereColumn(const QString &first, const QString &comparison,
                              const QString &second, const QString &condition)
{
    // Compile check for a invalid comparison operator
    invalidOperator(comparison);

    m_wheres.append({.column = first, .comparison = comparison, .condition = condition,
                     .type = WhereType::COLUMN, .columnTwo = second});

    return *this;
}

Builder &Builder::orWhereColumn(const QString &first, const QString &comparison,
                                const QString &second)
{
    return whereColumn(first, comparison, second, QStringLiteral("or"));
}

Builder &Builder::whereColumnEq(const QString &first, const QString &second,
                                const QString &condition)
{
    return whereColumn(first, QStringLiteral("="), second, condition);
}

Builder &Builder::orWhereColumnEq(const QString &first, const QString &second)
{
    return whereColumn(first, QStringLiteral("="), second, QStringLiteral("or"));
}

Builder &Builder::whereIn(const QString &column, const QVector<QVariant> &values,
                          const QString &condition, const bool nope)
{
    const auto type = nope ? WhereType::NOT_IN : WhereType::IN_;

    m_wheres.append({.column = column, .condition = condition, .type = type,
                     .values = values});

    /* Finally we'll add a binding for each values unless that value is an expression
       in which case we will just skip over it since it will be the query as a raw
       string and not as a parameterized place-holder to be replaced by the DB driver. */
    addBinding(cleanBindings(values), BindingType::WHERE);

    return *this;
}

Builder &Builder::orWhereIn(const QString &column, const QVector<QVariant> &values)
{
    return whereIn(column, values, QStringLiteral("or"));
}

Builder &Builder::whereNotIn(const QString &column, const QVector<QVariant> &values,
                             const QString &condition)
{
    return whereIn(column, values, condition, true);
}

Builder &Builder::orWhereNotIn(const QString &column, const QVector<QVariant> &values)
{
    return whereNotIn(column, values, QStringLiteral("or"));
}

Builder &Builder::whereNull(const QString &column, const QString &condition,
                            const bool nope)
{
    return whereNull(QStringList(column), condition, nope);
}

Builder &Builder::orWhereNull(const QString &column)
{
    return orWhereNull(QStringList(column));
}

Builder &Builder::whereNotNull(const QString &column, const QString &condition)
{
    return whereNotNull(QStringList(column), condition);
}

Builder &Builder::orWhereNotNull(const QString &column)
{
    return orWhereNotNull(QStringList(column));
}

Builder &Builder::whereNull(const QStringList &columns, const QString &condition,
                            const bool nope)
{
    const auto type = nope ? WhereType::NOT_NULL : WhereType::NULL_;

    for (const auto &column : columns)
        m_wheres.append({.column = column, .condition = condition, .type = type});

    return *this;
}

Builder &Builder::orWhereNull(const QStringList &columns)
{
    return whereNull(columns, QStringLiteral("or"));
}

Builder &Builder::whereNotNull(const QStringList &columns, const QString &condition)
{
    return whereNull(columns, condition, true);
}

Builder &Builder::orWhereNotNull(const QStringList &columns)
{
    return whereNotNull(columns, QStringLiteral("or"));
}

Builder &Builder::groupBy(const QStringList &groups)
{
    if (groups.isEmpty())
        return *this;

    std::copy(groups.cbegin(), groups.cend(), std::back_inserter(m_groups));

    return *this;
}

Builder &Builder::groupBy(const QString &group)
{
    return groupBy(QStringList {group});
}

Builder &Builder::having(const QString &column, const QString &comparison,
                         const QVariant &value, const QString &condition)
{
    // Compile check for a invalid comparison operator
    invalidOperator(comparison);

    m_havings.append({column, value, comparison, condition, HavingType::BASIC});
    addBinding(value, BindingType::HAVING);

    return *this;
}

Builder &Builder::orHaving(const QString &column, const QString &comparison,
                           const QVariant &value)
{
    return having(column, comparison, value, QStringLiteral("or"));
}

Builder &Builder::orderBy(const QString &column, const QString &direction)
{
    const auto &directionLower = direction.toLower();

    if ((directionLower != "asc") && (directionLower != "desc"))
        throw RuntimeError("Order direction must be \"asc\" or \"desc\", "
                           "case is not important.");

    m_orders.append({column, directionLower});

    return *this;
}

Builder &Builder::orderByDesc(const QString &column)
{
    return orderBy(column, QStringLiteral("desc"));
}

Builder &Builder::latest(const QString &column)
{
    /* Default value "created_at" is ok, because we are in the QueryBuilder,
       in the Model/TinyBuilder is this default argument processed by
       the TinyBuilder::getCreatedAtColumnForLatestOldest() method. */
    return orderBy(column, QStringLiteral("desc"));
}

Builder &Builder::oldest(const QString &column)
{
    return orderBy(column, QStringLiteral("asc"));
}

Builder &Builder::reorder()
{
    m_orders.clear();

    m_bindings.find(BindingType::ORDER)->clear();

    return *this;
}

Builder &Builder::reorder(const QString &column, const QString &direction)
{
    reorder();

    return orderBy(column, direction);
}

Builder &Builder::limit(const int value)
{
    /* I checked negative limit/offset, MySQL and PostgreSQL throws an error,
       SQLite accepts a negative value, but it has no effect and Microsoft SQL Server
       doesn't support negative values too, as is described here:
       https://bit.ly/3yrG7aF */
    Q_ASSERT(value >= 0);

    if (value >= 0)
        m_limit = value;

    return *this;
}

Builder &Builder::take(const int value)
{
    return limit(value);
}

Builder &Builder::offset(const int value)
{
    Q_ASSERT(value >= 0);

    m_offset = std::max(0, value);

    return *this;
}

Builder &Builder::skip(const int value)
{
    return offset(value);
}

Builder &Builder::forPage(const int page, const int perPage)
{
    return offset((page - 1) * perPage).limit(perPage);
}

Builder &Builder::lockForUpdate()
{
    return lock(true);
}

Builder &Builder::sharedLock()
{
    return lock(false);
}

Builder &Builder::lock(const bool value)
{
    m_lock = value;

    // FEATURE read/write connection silverqx
//    if (! is_null($this->lock))
//        useWritePdo();

    return *this;
}

Builder &Builder::lock(const char *value)
{
    /* I need this overload because if I pass 'char *' string to the lock(), the compiler
       selects lock(bool) overload, this behavior is described here:
       https://stackoverflow.com/questions/14770252/string-literal-matches-bool-overload-instead-of-stdstring */
    m_lock = QString(value);

    return *this;
}

Builder &Builder::lock(const QString &value)
{
    m_lock = value;

    return *this;
}

Builder &Builder::lock(QString &&value)
{
    m_lock = std::move(value);

    return *this;
}

QVector<QVariant> Builder::getBindings() const
{
    QVector<QVariant> flattenBindings;

    std::for_each(m_bindings.cbegin(), m_bindings.cend(),
                  [&flattenBindings](const auto &bindings)
    {
        for (const auto &binding : bindings)
            flattenBindings.append(binding);
    });

    return flattenBindings;
}

// TODO next revisit QSharedPointer, after few weeks I'm pretty sure that this can/should be std::unique_pre, like in the TinyBuilder, I need to check if more instances need to save this pointer at once, if don't then I have to change it silverqx
QSharedPointer<Builder> Builder::newQuery() const
{
    return QSharedPointer<Builder>::create(m_connection, m_grammar);
}

QSharedPointer<Builder> Builder::forNestedWhere() const
{
    // Ownership of the QSharedPointer
    const auto query = newQuery();

    query->setFrom(m_from);

    return query;
}

Expression Builder::raw(const QVariant &value) const
{
    return m_connection.raw(value);
}

// TODO now, (still need to be revisited) it can be reference, shared owner will be callee, and copy will be made during m_wheres.append() silverqx
Builder &Builder::addNestedWhereQuery(const QSharedPointer<Builder> &query,
                                      const QString &condition)
{
    if (!(query->m_wheres.size() > 0))
        return *this;

    m_wheres.append({.column = {}, .condition = condition, .type = WhereType::NESTED,
                     .nestedQuery = query});

    const auto &whereBindings =
            query->getRawBindings().find(BindingType::WHERE).value();

    if (whereBindings.size() > 0)
        addBinding(whereBindings, BindingType::WHERE);

    return *this;
}

bool Builder::invalidOperator(const QString &comparison) const
{
    const auto comparison_ = comparison.toLower();

    return !m_operators.contains(comparison_) &&
            !m_grammar.getOperators().contains(comparison_);
}

Builder &Builder::addBinding(const QVariant &binding, const BindingType type)
{
    if (!m_bindings.contains(type))
        // TODO add hash which maps BindingType to the QString silverqx
        throw RuntimeError(QStringLiteral("Invalid binding type: %1")
                           .arg(static_cast<int>(type)));

    m_bindings[type].append(binding);

    return *this;
}

Builder &Builder::addBinding(const QVector<QVariant> &bindings, const BindingType type)
{
    // TODO duplicate check, unify silverqx
    if (!m_bindings.contains(type))
        // TODO add hash which maps BindingType to the QString silverqx
        throw RuntimeError(QStringLiteral("Invalid binding type: %1")
                           .arg(static_cast<int>(type)));

    std::copy(bindings.cbegin(), bindings.cend(), std::back_inserter(m_bindings[type]));

    return *this;
}

Builder &Builder::addBinding(QVector<QVariant> &&bindings, const BindingType type)
{
    if (!m_bindings.contains(type))
        throw RuntimeError(QStringLiteral("Invalid binding type: %1")
                           .arg(static_cast<int>(type)));

    std::move(bindings.begin(), bindings.end(), std::back_inserter(m_bindings[type]));

    return *this;
}

// TODO investigate extended lifetime of reference in cleanBindings(), important case 🤔 silverqx
QVector<QVariant> Builder::cleanBindings(const QVector<QVariant> &bindings) const
{
    // TODO investigate const, move, reserve() vs ctor(size), nice example of move semantics 😏 silverqx
    // FUTURE rewrite with ranges transform, for fun silverqx
    QVector<QVariant> cleanedBindings;
    cleanedBindings.reserve(bindings.size());

    for (const auto &binding : bindings)
        if (!binding.canConvert<Expression>())
            cleanedBindings.append(binding);

    return cleanedBindings;
}

Builder &
Builder::addArrayOfWheres(const QVector<WhereItem> &values, const QString &condition)
{
    return where([&values, &condition](Builder &query)
    {
        for (const auto &where : values)
            query.where(where.column, where.comparison, where.value,
                        where.condition.isEmpty() ? condition : where.condition);

    }, condition);
}

Builder &
Builder::addArrayOfWheres(const QVector<WhereColumnItem> &values,
                          const QString &condition)
{
    // WARN condition also affects condition in QVector, I don't like it silverqx
    return where([&values, &condition](Builder &query)
    {
        for (const auto &where : values)
            query.whereColumn(where.first, where.comparison, where.second,
                              where.condition.isEmpty() ? condition : where.condition);

    }, condition);
}

// CUR revisit QSharedPointer, here it should be unique_prt absolutely silverqx
QSharedPointer<JoinClause>
Builder::newJoinClause(const Builder &query, const QString &type,
                       const QString &table) const
{
    return QSharedPointer<JoinClause>::create(query, type, table);
}

QSharedPointer<JoinClause>
Builder::newJoinClause(const Builder &query, const QString &type,
                       Expression &&table) const
{
    return QSharedPointer<JoinClause>::create(query, type, std::move(table));
}

Builder &Builder::clearColumns()
{
    m_columns.clear();

    m_bindings[BindingType::SELECT].clear();

    return *this;
}

QSqlQuery
Builder::onceWithColumns(
            const QStringList &columns,
            const std::function<QSqlQuery()> &callback)
{
    // Save orignal columns
    const auto original = m_columns;

    if (original.isEmpty())
        m_columns = columns;

    const auto result = std::invoke(callback);

    // After running the callback, the columns are reset to the original value
    m_columns = original;

    return result;
}

std::pair<QString, QVector<QVariant>>
Builder::createSub(const std::function<void(Builder &)> &callback) const
{
    // Ownership of the QSharedPointer<QueryBuilder>
    const auto query = forSubQuery();

    std::invoke(callback, *query);

    prependDatabaseNameIfCrossDatabaseQuery(*query);

    return {query->toSql(), query->getBindings()};
}

std::pair<QString, QVector<QVariant>>
Builder::createSub(Builder &query) const
{
    prependDatabaseNameIfCrossDatabaseQuery(query);

    return {query.toSql(), query.getBindings()};
}

std::pair<QString, QVector<QVariant>>
Builder::createSub(const QString &query) const
{
    return {query, {}};
}

std::pair<QString, QVector<QVariant>>
Builder::createSub(QString &&query) const
{
    return {std::move(query), {}};
}

Builder &Builder::prependDatabaseNameIfCrossDatabaseQuery(Builder &query) const
{
    const auto &queryDatabaseName = query.getConnection().getDatabaseName();
    const auto queryFrom = std::get<QString>(query.m_from);

    if (queryDatabaseName != getConnection().getDatabaseName() &&
        !queryFrom.startsWith(queryDatabaseName) &&
        !queryFrom.contains(QChar('.'))
    )
        query.from(QStringLiteral("%1.%2").arg(queryDatabaseName, queryFrom));

    return query;
}

QSqlQuery Builder::runSelect()
{
    return m_connection.select(toSql(), getBindings());
}

Builder &Builder::joinInternal(
            QSharedPointer<JoinClause> &&join, const QString &first,
            const QString &comparison, const QVariant &second, const bool where)
{
    if (where)
        join->where(first, comparison, second);
    else
        join->on(first, comparison, second.value<QString>());

    // Move ownership
    return joinInternal(std::move(join));
}

Builder &Builder::joinInternal(QSharedPointer<JoinClause> &&join,
                               const std::function<void(JoinClause &)> &callback)
{
    std::invoke(callback, *join);

    // Move ownership
    return joinInternal(std::move(join));
}

Builder &Builder::joinInternal(QSharedPointer<JoinClause> &&join)
{
    // For convenience, I want to append first and afterwards add bindings
    const auto &joinRef = *join;

    // CUR revisit QSharedPointer, here it should be unique_prt absolutely silverqx
    // Move ownership
    m_joins.append(std::move(join));

    addBinding(joinRef.getBindings(), BindingType::JOIN);

    return *this;
}

Builder &Builder::joinSubInternal(
            std::pair<QString, QVector<QVariant>> &&subQuery, const QString &as,
            const QString &first, const QString &comparison, const QVariant &second,
            const QString &type, const bool where)
{
    auto &[queryString, bindings] = subQuery;

    addBinding(std::move(bindings), BindingType::JOIN);

    return join(Expression(QStringLiteral("(%1) as %2").arg(queryString,
                                                            m_grammar.wrapTable(as))),
                first, comparison, second, type, where);
}

} // namespace Orm
#ifdef TINYORM_COMMON_NAMESPACE
} // namespace TINYORM_COMMON_NAMESPACE
#endif
