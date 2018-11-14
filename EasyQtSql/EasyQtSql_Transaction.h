#ifndef EASYQTSQL_TRANSACTION_H
#define EASYQTSQL_TRANSACTION_H

/*
 * The MIT License (MIT)
 * Copyright 2018 Alexey Kramin
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
*/

#ifndef EASY_QT_SQL_MAIN

#include <QtSql>
#include "EasyQtSql_DBException.h"
#include "EasyQtSql_NonQueryResult.h"
#include "EasyQtSql_InsertQuery.h"
#include "EasyQtSql_DeleteQuery.h"
#include "EasyQtSql_UpdateQuery.h"
#include "EasyQtSql_PreparedQuery.h"

#endif


/*!
\brief QSqlDatabase wrapper.

\code
void test()
{
   QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
   db.setDatabaseName(":memory:");

   try
   {
      Database sdb(db);

      sdb.execNonQuery("CREATE TABLE table (a int, b int, c int, d text)");
   }
   catch (const DBException &e)
   {
      //you can handle all the errors at one point

      qDebug() << e.lastError << e.lastQuery;
   }
}
\endcode
*/
class Database
{
   Q_DISABLE_COPY(Database)

public:

   /*!
    * \param db QSqlDatabase to use
    *
    * Creates an Database object, tries to open <em>db</em> connection if not opened.
    *
    * \throws DBException
    */
   Database (const QSqlDatabase &db = QSqlDatabase())
   {
      m_db = db.isValid() ? db : QSqlDatabase::database();

      if (!m_db.isOpen())
      {
         if (!m_db.open())
         {
            #ifdef DB_EXCEPTIONS_ENABLED
            throw DBException(m_db);
            #endif
         }
      }
   }

   /*!
    * \brief Returns information about the last error that occurred on the underlying database.
    */
   QSqlError lastError() const
   {
      return m_db.lastError();
   }

   /*!
   \brief Executes non-query SQL statement (DELETE, INSERT, UPDATE, CREATE, ALTER, etc.)
   \param query SQL statement string
   \throws DBException
   */
   NonQueryResult execNonQuery(const QString &sql) const
   {
      QSqlQuery q = m_db.exec(sql);

#ifdef DB_EXCEPTIONS_ENABLED

      QSqlError lastError = q.lastError();

      if (lastError.isValid())
         throw DBException(q);

#endif

      return NonQueryResult(q);
   }

   /*!
   \brief Executes SELECT query
   \param query SQL statement string
   \throws DBException
   */
   QueryResult execQuery(const QString &sql) const
   {
      QSqlQuery q = m_db.exec(sql);

#ifdef DB_EXCEPTIONS_ENABLED

      QSqlError lastError = q.lastError();

      if (lastError.isValid())
         throw DBException(q);

#endif

      return QueryResult(q);
   }

   /*!
   \brief Creates INSERT query wrapper
   \param table Table to insert into with list of columns
   */
   InsertQuery insertInto(const QString &table) const
   {
      InsertQuery query(table, m_db);

      return query;
   }

   /*!
   \brief Creates DELETE query wrapper
   \param table Table to delete from
   */
   DeleteQuery deleteFrom(const QString &table) const
   {
      DeleteQuery query(table, m_db);

      return query;
   }

   /*!
   \brief Creates UPDATE query wrapper
   \param table Table to update
   */
   UpdateQuery update(const QString &table) const
   {
      UpdateQuery query(table, m_db);

      return query;
   }

   /*!
   \brief Prepares SQL statement
   \param sql SQL statement string
   \param forwardOnly Configure underlying QSqlQuery as forwardOnly
   */
   PreparedQuery prepare(const QString &sql, bool forwardOnly = true) const
   {
      PreparedQuery query(sql, m_db, forwardOnly);

      return query;
   }

   /*!
    * \brief Returns a reference to the wrapped QSqlDatabase object
    */
   QSqlDatabase &qSqlDatabase()
   {
      return m_db;
   }

protected:
   QSqlDatabase m_db;
};


/*!
\brief QSqlDatabase transaction wrapper.

Features:
 - Automatic rollback of non-expclicitely commited transactions
 - Helper methods: Transaction::execNonQuery, Transaction::execQuery, Transaction::insertInto, Transaction::deleteFrom, Transaction::update, Transaction::prepare.

\code
void test()
{
   QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
   db.setDatabaseName(":memory:");
   db.open();

   try
   {
      Transaction t(db);

      t.execNonQuery("CREATE TABLE table (a int, b int, c int, d text)");

      t.insertInto("table (a, b, c, d)")
         .values(1, 2, 3, "row1")
         .values(4, 5, 6, "row2")
         .values(7, 8, 9, "row3")
         .exec();

      PreparedQuery query = t.prepare("SELECT a, b, c, d FROM table");

      QueryResult res = query.exec();
      while (res.next())
      {
         QVariantMap map = res.toMap();
         qDebug() << map;
      }

      t.update("table")
         .set("a", 111)
         .set("b", 222)
         .where("c = ? OR c = ?", 3, 6);

      res = query.exec();
      while (res.next())
      {
         QVariantMap map = res.toMap();
         qDebug() << map;
      }

      t.commit(); //the transaction will be rolled back on exit from the scope (when calling the destructor) if you do not explicitly commit

   catch (const DBException &e)
   {
      //you can handle all the errors at one point
      //the transaction will be automatically rolled back on exception

      qDebug() << e.lastError << e.lastQuery;
   }
}
\endcode
*/
class  Transaction : public Database
{
   Q_DISABLE_COPY(Transaction)

public:

   Transaction (const QSqlDatabase &db = QSqlDatabase())
     : Database(db)
     , m_commited(false)
     , m_started(false)
   {      
      m_started = m_db.transaction();

      #ifdef DB_EXCEPTIONS_ENABLED
      if (!m_started)
      {
         throw DBException(m_db);
      }
      #endif
   }

   ~Transaction()
   {
      if (!m_commited)
      {
         m_db.rollback();
      }
   }

   /*!
   \brief Commits transaction

   The transaction will be rolled back on calling the destructor if not explicitly commited

   \throws DBException
   */
   bool commit()
   {
      if (!m_commited)
      {
         m_commited = m_db.commit();

#ifdef DB_EXCEPTIONS_ENABLED

         if (!m_commited)
            throw DBException(m_db);

#endif

      }

      return m_commited;
   }

   /*!
   \brief Rolls back transaction
   */
   bool rollback()
   {
      bool res = false;

      if (!m_commited)
      {
         res = m_db.rollback();

         m_commited = false;
      }

      return res;
   }

   /*!
   \brief Returns true if the transaction has been started successfully. Otherwise it returns false.
   */
   bool started() const
   {
      return m_started;
   }

   /*!
   \brief Returns true if the transaction has been commited successfully. Otherwise it returns false.
   */
   bool commited() const
   {
      return m_commited;
   }

private:   
   bool m_commited = false;
   bool m_started = false;
};

#endif // EASYQTSQL_TRANSACTION_H