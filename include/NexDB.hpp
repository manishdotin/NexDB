#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace nexdb {

enum class DataType { Int, Double, Text, Bool };

enum class Operator { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

using Value = std::variant<int64_t, double, std::string, bool>;

struct Column {
    std::string name;
    DataType type;
};
using Row = std::vector<Value>;

std::string toString(const Value& value);
std::string escape(const std::string& text);
std::string unescape(const std::string& text);
DataType dataTypeFromString(const std::string& text);
std::string dataTypeToString(DataType type);
Value parseValue(const std::string& token, DataType type);

struct Condition {
    std::string column;
    Operator op;
    Value value;
};

class Table {
public:
    Table() = default;
    Table(std::string name, std::vector<Column> columns);

    const std::string& name() const noexcept { return name_; }
    const std::vector<Column>& columns() const noexcept { return columns_; }
    const std::vector<Row>& rows() const noexcept { return rows_; }

    void insert(Row row);
    std::size_t update(const Condition& condition, std::size_t columnIndex, Value newValue);
    std::size_t remove(const std::optional<Condition>& condition);
    std::vector<std::size_t> select(const std::optional<Condition>& condition) const;

    int columnIndex(const std::string& name) const;
    const Value& valueAt(std::size_t row, std::size_t column) const;
    void loadRows(std::vector<Row> rows);

private:
    std::string name_;
    std::vector<Column> columns_;
    std::vector<Row> rows_;

    bool matches(const Row& row, const Condition& condition) const;
};

class Database {
public:
    void createTable(const std::string& name, std::vector<Column> columns);
    void dropTable(const std::string& name);
    Table& table(const std::string& name);
    const Table& table(const std::string& name) const;
    bool hasTable(const std::string& name) const;
    std::vector<std::string> tableNames() const;
    void clear();

private:
    std::map<std::string, Table> tables_;
};

struct ParsedCommand {
    enum class Type { Create, Insert, Select, Update, Delete, Tables, Schema, Help, Save, Exit, Drop, Unknown };
    Type type = Type::Unknown;
    std::string table;
    std::vector<Column> columns;
    std::vector<std::string> rawValues;
    std::optional<Condition> condition;
    std::string updateColumn;
    std::string updateValue;
};

class Parser {
public:
    ParsedCommand parse(const std::string& input) const;

private:
    static std::vector<std::string> tokenize(const std::string& input);
    static std::string upper(std::string s);
    static std::string trim(std::string s);
    static Operator parseOperator(const std::string& token);
};

class Storage {
public:
    explicit Storage(std::filesystem::path root = "data");
    void save(const Database& db) const;
    void load(Database& db) const;
    const std::filesystem::path& root() const noexcept { return root_; }

private:
    std::filesystem::path root_;
};

class Engine {
public:
    explicit Engine(std::filesystem::path dataDir = "data") : storage_(std::move(dataDir)) {}

    void execute(const ParsedCommand& cmd, bool interactive = true);
    void run();
    Database& database() noexcept { return db_; }
    const Database& database() const noexcept { return db_; }
    Storage& storage() noexcept { return storage_; }

private:
    Database db_;
    Storage storage_;
    Parser parser_;

    void printRows(const Table& table, const std::vector<std::size_t>& indices) const;
    void printHelp() const;
    static std::string trim(const std::string& s);
};

}
