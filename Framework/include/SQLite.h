#ifndef _SQLITE_H
#define _SQLITE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string>

#include "sqlite3/sqlite3.h"

using namespace std;

class CSQLite
{
public:
  CSQLite();
  ~CSQLite();

  int OpenDB(const char *fileName);
  int CloseDB();
  int ExecuteSQL(const char *sql);
  int CreateTable(const char *tableName, const char *columns);
  int DropTable(const char *tableName);
  int Insert(const char *tableName, const char *columns, const char *values);
  int Delete(const char *tableName, const char *where);
  int Update(const char *tableName, const char *set, const char *where);
  int GetTable(const char *tableName, const char *columns, const char *filter,
               char **&table, int &row, int &col);
  int FreeTable(char **&table);

private:
  sqlite3 *pDB;
  pthread_mutex_t operateMutex;
};

#endif