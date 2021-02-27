/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_SQL_STORE_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_SQL_STORE_H_

#include <string>
#include <utility>
#include <vector>

#include "bat/ledger/internal/core/async_result.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"
#include "bat/ledger/public/interfaces/ledger.mojom.h"

namespace ledger {

// Provides methods for accessing the result of an SQL operation. |SQLReader|
// implements a subset of the interface defined by |sql::Statement|. Do not add
// public methods to this class that are not present in |sql::Statement|.
//
// Example:
//   SQLReader reader(db_response);
//   if (reader.Step()) {
//     std::string value = reader.ColumnString(0);
//   }
//
// Prefer to use |SQLReader| over directly accessing |DBCommandResponsePtr|.
class SQLReader {
 public:
  explicit SQLReader(const mojom::DBCommandResponse* response);
  explicit SQLReader(const mojom::DBCommandResponsePtr& response);
  ~SQLReader();

  SQLReader(const SQLReader&) = delete;
  SQLReader& operator=(const SQLReader&) = delete;

  // Advances the reader and returns a value indicating whether the reader is
  // currently positioned on a record.
  bool Step();

  // Returns a valid indicating whether the SQL command succeeded.
  bool Succeeded() const;

  // Reads the value of the specified column. If the requested type does not
  // match the underlying value type a conversion is performed. Similar to
  // |sql::Statement|, string-to-number conversions are best-effort.
  bool ColumnBool(int col) const;
  int64_t ColumnInt64(int col) const;
  double ColumnDouble(int col) const;
  std::string ColumnString(int col) const;

 private:
  mojom::DBValue* GetDBValue(int col) const;

  const mojom::DBCommandResponse* response_;
  int row_ = -1;
};

// Provides access to the BAT ledger SQLite database.
//
// Example:
//   context()
//       .Get<SQLStore>();
//       .Query("SELECT amount from transaction WHERE id = ?", id)
//       .Then(base::BindOnce(...));
class SQLStore : public BATLedgerContext::Object {
 public:
  static const size_t kComponentKey;

  using CommandResult = AsyncResult<mojom::DBCommandResponsePtr>;

  // Opens the database and returns the current database version number.
  CommandResult Open(int current_version);

  // Closes the database.
  CommandResult Close();

  // Runs a list of commands as part of a database version migration.
  CommandResult RunMigration(int version,
                             std::vector<mojom::DBCommandPtr> commands);

  // Clears free space in the database.
  CommandResult Vacuum();

  // Runs a list of commands as a transaction against the database
  CommandResult RunTransaction(std::vector<mojom::DBCommandPtr> commands);

  // Runs a command against the database.
  CommandResult RunTransaction(mojom::DBCommandPtr command);

  // Runs a command against the database, using a series of values as command
  // bindings.
  template <typename... Args>
  CommandResult Execute(const std::string& sql, Args&&... args) {
    return RunTransaction(CreateCommand(sql, std::forward<Args>(args)...));
  }

  // Executes a query against the database, using a series of values as command
  // bindings.
  template <typename... Args>
  CommandResult Query(const std::string& sql, Args&&... args) {
    return RunTransaction(CreateQuery(sql, std::forward<Args>(args)...));
  }

  // Returns a |DBValuePtr| that wraps the specified input value.
  template <typename T>
  static mojom::DBValuePtr Bind(T&& value) {
    auto db_value = mojom::DBValue::New();
    SetDBValue(db_value.get(), std::forward<T>(value));
    return db_value;
  }

  // Creates a database command from a SQL string and a series of SQL command
  // binding values.
  template <typename... Args>
  static mojom::DBCommandPtr CreateCommand(const std::string& sql,
                                           Args&&... args) {
    auto command = mojom::DBCommand::New();
    command->type = mojom::DBCommand::Type::RUN;
    command->command = sql;
    command->bindings = BindValues(std::forward<Args>(args)...);
    return command;
  }

  // Creates a database query command from a SQL string and a series of SQL
  // command binding values.
  template <typename... Args>
  static mojom::DBCommandPtr CreateQuery(const std::string& sql,
                                         Args&&... args) {
    auto command = CreateCommand(sql, std::forward<Args>(args)...);
    command->type = mojom::DBCommand::Type::READ;
    return command;
  }

 protected:
  virtual CommandResult RunTransactionImpl(mojom::DBTransactionPtr transaction);

 private:
  static void SetDBValue(mojom::DBValue* db_value, double value);
  static void SetDBValue(mojom::DBValue* db_value, int32_t value);
  static void SetDBValue(mojom::DBValue* db_value, int64_t value);
  static void SetDBValue(mojom::DBValue* db_value, bool value);
  static void SetDBValue(mojom::DBValue* db_value, const char* value);
  static void SetDBValue(mojom::DBValue* db_value, const std::string& value);
  static void SetDBValue(mojom::DBValue* db_value, std::nullptr_t);
  static void SetDBValue(mojom::DBValue* db_value, mojom::DBValuePtr);

  template <typename... Args>
  static std::vector<mojom::DBCommandBindingPtr> BindValues(Args&&... args) {
    std::vector<mojom::DBCommandBindingPtr> bindings;
    AddBindings(&bindings, std::forward<Args>(args)...);
    return bindings;
  }

  template <typename T>
  static void AddBindings(T* bindings) {}

  template <typename T, typename U, typename... Args>
  static void AddBindings(T* bindings, U&& value, Args&&... args) {
    auto binding = mojom::DBCommandBinding::New();
    binding->index = bindings->size();
    binding->value = Bind(std::forward<U>(value));
    bindings->push_back(std::move(binding));
    AddBindings(bindings, std::forward<Args>(args)...);
  }
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_SQL_STORE_H_
