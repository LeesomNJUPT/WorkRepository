#include "../include/SQLite.h"

CSQLite::CSQLite()
{
    pDB = NULL;
    pthread_mutex_init(&operateMutex, NULL);
}

CSQLite::~CSQLite()
{
    CloseDB();
    pthread_mutex_destroy(&operateMutex);
}

int CSQLite::OpenDB(const char *fileName)
{
    pthread_mutex_lock(&operateMutex);
    if (NULL != pDB)
    {
        printf("CSQLite::OpenDB(): Repeatedly open database.\n");
        pthread_mutex_unlock(&operateMutex);
        return -1;
    }

    int ret = sqlite3_open(fileName, &pDB);
    if (SQLITE_OK != ret)
        printf("CSQLite::OpenDB(): %s.\n", sqlite3_errmsg(pDB));

    pthread_mutex_unlock(&operateMutex);
    return ret;
}

int CSQLite::CloseDB()
{
    pthread_mutex_lock(&operateMutex);
    if (NULL != pDB)
    {
        sqlite3_close(pDB);
        pDB = NULL;
    }
    pthread_mutex_unlock(&operateMutex);
    return 0;
}

int CSQLite::ExecuteSQL(const char *sql)
{
    pthread_mutex_lock(&operateMutex);
    if (NULL == pDB)
    {
        printf("CSQLite::ExecuteSQL(): Database not open.\n");
        pthread_mutex_unlock(&operateMutex);
        return -1;
    }

    char *errMsg = NULL;
    int ret = sqlite3_exec(pDB, sql, NULL, NULL, &errMsg);
    if (SQLITE_OK != ret)
    {
        printf("CSQLite::ExecuteSQL(): %s.\n", errMsg);
        sqlite3_free(errMsg);
    }

    pthread_mutex_unlock(&operateMutex);
    return ret;
}

int CSQLite::CreateTable(const char *tableName, const char *columns)
{
    string strTableName = tableName;
    string strColumns = columns;
    string sql;
    sql = "CREATE TABLE " + strTableName + " (" + strColumns + ")";

    return ExecuteSQL(sql.c_str());
}

int CSQLite::DropTable(const char *tableName)
{
    string strTableName = tableName;
    string sql = "DROP TABLE IF EXISTS " + strTableName;

    return ExecuteSQL(sql.c_str());
}

int CSQLite::Insert(const char *tableName, const char *columns, const char *values)
{
    string strTableName = tableName;
    string strColumns = columns;
    string strValues = values;
    string sql = "INSERT INTO " + strTableName + " (" + strColumns + ") VALUES (" + strValues + ")";

    return ExecuteSQL(sql.c_str());
}

int CSQLite::Delete(const char *tableName, const char *where)
{
    string strTableName = tableName;

    string sql;
    if (where)
    {
        string strWhere = where;
        sql = "DELETE FROM " + strTableName + " WHERE " + strWhere;
    }
    else
    {
        sql = "DELETE FROM " + strTableName;
    }

    return ExecuteSQL(sql.c_str());
}

int CSQLite::Update(const char *tableName, const char *set, const char *where)
{
    string strTableName = tableName;
    string strSet = set;
    string strWhere = where;
    string sql = "UPDATE " + strTableName + " SET " + strSet + " WHERE " + strWhere;

    return ExecuteSQL(sql.c_str());
}

int CSQLite::GetTable(const char *tableName, const char *columns, const char *filter,
                      char **&table, int &row, int &col)
{
    pthread_mutex_lock(&operateMutex);
    if (NULL == pDB)
    {
        printf("CSQLite::GetTable(): Database not open.\n");
        pthread_mutex_unlock(&operateMutex);
        return -1;
    }
    string strTableName;
    string strColumns;
    string sql;

    if (tableName)
        strTableName = tableName;
    if (columns)
        strColumns = columns;
    if (filter)
    {
        string strFilter = filter;
        sql = "SELECT " + strColumns + " FROM " + strTableName + " " + strFilter;
    }
    else
    {
        sql = "SELECT " + strColumns + " FROM " + strTableName;
    }

    char *errMsg = NULL;
    int ret = sqlite3_get_table(pDB, sql.c_str(), &table, &row, &col, &errMsg);
    if (ret != SQLITE_OK)
    {
        printf("CSQLite::GetTable(): %s.\n", errMsg);
        sqlite3_free(errMsg);
    }

    pthread_mutex_unlock(&operateMutex);
    return ret;
}

int CSQLite::FreeTable(char **&table)
{
    if (NULL != table)
    {
        sqlite3_free_table(table);
        table = NULL;
    }

    return 0;
}