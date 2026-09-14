#include "NexDB.hpp"

namespace nexdb {

namespace {

bool numeric(const Value& v) {
    return std::holds_alternative<int64_t>(v) || std::holds_alternative<double>(v);
}

double number(const Value& v) {
    if (std::holds_alternative<int64_t>(v)) return static_cast<double>(std::get<int64_t>(v));
    return std::get<double>(v);
}

bool compare(const Value& lhs, Operator op, const Value& rhs) {
    if (numeric(lhs) && numeric(rhs)) {
        const double a = number(lhs), b = number(rhs);
        switch (op) {
            case Operator::Equal: return a == b;
            case Operator::NotEqual: return a != b;
            case Operator::Less: return a < b;
            case Operator::LessEqual: return a <= b;
            case Operator::Greater: return a > b;
            case Operator::GreaterEqual: return a >= b;
        }
    }
    if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs)) {
        const auto& a = std::get<std::string>(lhs); const auto& b = std::get<std::string>(rhs);
        switch (op) {
            case Operator::Equal: return a == b;
            case Operator::NotEqual: return a != b;
            case Operator::Less: return a < b;
            case Operator::LessEqual: return a <= b;
            case Operator::Greater: return a > b;
            case Operator::GreaterEqual: return a >= b;
        }
    }
    if (std::holds_alternative<bool>(lhs) && std::holds_alternative<bool>(rhs)) {
        const bool a = std::get<bool>(lhs), b = std::get<bool>(rhs);
        switch (op) {
            case Operator::Equal: return a == b;
            case Operator::NotEqual: return a != b;
            default: throw std::runtime_error("Only = and != are supported for BOOL values");
        }
    }
    throw std::runtime_error("Incompatible values in condition");
}

} // namespace

std::string toString(const Value& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, double>) {
            std::ostringstream out; out << std::setprecision(15) << v; return out.str();
        } else if constexpr (std::is_same_v<T, std::string>) return v;
        else return std::to_string(v);
    }, value);
}

std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\' || c == '|') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string unescape(const std::string& text) {
    std::string out; bool escaped = false;
    for (char c : text) {
        if (escaped) { out.push_back(c); escaped = false; }
        else if (c == '\\') escaped = true;
        else out.push_back(c);
    }
    if (escaped) out.push_back('\\');
    return out;
}

DataType dataTypeFromString(const std::string& text) {
    std::string t; for (char c : text) t += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (t == "INT" || t == "INTEGER") return DataType::Int;
    if (t == "DOUBLE" || t == "REAL") return DataType::Double;
    if (t == "TEXT" || t == "STRING") return DataType::Text;
    if (t == "BOOL" || t == "BOOLEAN") return DataType::Bool;
    throw std::runtime_error("Unknown data type: " + text);
}

std::string dataTypeToString(DataType type) {
    switch (type) {
        case DataType::Int: return "INT";
        case DataType::Double: return "DOUBLE";
        case DataType::Text: return "TEXT";
        case DataType::Bool: return "BOOL";
    }
    return "UNKNOWN";
}

Value parseValue(const std::string& token, DataType type) {
    try {
        switch (type) {
            case DataType::Int: return static_cast<int64_t>(std::stoll(token));
            case DataType::Double: return std::stod(token);
            case DataType::Text: return token;
            case DataType::Bool: {
                std::string t; for (char c : token) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (t == "true") return true;
                if (t == "false") return false;
                throw std::runtime_error("Expected TRUE or FALSE");
            }
        }
    } catch (const std::invalid_argument&) { throw std::runtime_error("Invalid value: " + token); }
    catch (const std::out_of_range&) { throw std::runtime_error("Value out of range: " + token); }
    throw std::runtime_error("Invalid data type");
}

Table::Table(std::string name, std::vector<Column> columns) : name_(std::move(name)), columns_(std::move(columns)) {
    if (name_.empty() || columns_.empty()) throw std::runtime_error("Table requires a name and at least one column");
    std::unordered_map<std::string, bool> seen;
    for (const auto& c : columns_) {
        if (c.name.empty()) throw std::runtime_error("Column name cannot be empty");
        if (seen[c.name]) throw std::runtime_error("Duplicate column: " + c.name);
        seen[c.name] = true;
    }
}

void Table::insert(Row row) {
    if (row.size() != columns_.size()) throw std::runtime_error("Column count does not match table schema");
    rows_.push_back(std::move(row));
}

int Table::columnIndex(const std::string& name) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) if (columns_[i].name == name) return static_cast<int>(i);
    throw std::runtime_error("Unknown column: " + name);
}

const Value& Table::valueAt(std::size_t row, std::size_t column) const {
    if (row >= rows_.size() || column >= columns_.size()) throw std::out_of_range("Invalid row or column");
    return rows_[row][column];
}

bool Table::matches(const Row& row, const Condition& condition) const {
    const int idx = columnIndex(condition.column);
    return compare(row[static_cast<std::size_t>(idx)], condition.op, condition.value);
}

std::vector<std::size_t> Table::select(const std::optional<Condition>& condition) const {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < rows_.size(); ++i) if (!condition || matches(rows_[i], *condition)) result.push_back(i);
    return result;
}

std::size_t Table::update(const Condition& condition, std::size_t columnIndexValue, Value newValue) {
    if (columnIndexValue >= columns_.size()) throw std::runtime_error("Invalid update column");
    std::size_t count = 0;
    for (auto& row : rows_) if (matches(row, condition)) { row[columnIndexValue] = newValue; ++count; }
    return count;
}

std::size_t Table::remove(const std::optional<Condition>& condition) {
    const auto old = rows_.size();
    if (!condition) rows_.clear();
    else rows_.erase(std::remove_if(rows_.begin(), rows_.end(), [&](const Row& r) { return matches(r, *condition); }), rows_.end());
    return old - rows_.size();
}

void Table::loadRows(std::vector<Row> rows) {
    for (const auto& row : rows) if (row.size() != columns_.size()) throw std::runtime_error("Corrupt stored row");
    rows_ = std::move(rows);
}

void Database::createTable(const std::string& name, std::vector<Column> columns) {
    if (tables_.count(name)) throw std::runtime_error("Table already exists: " + name);
    tables_.emplace(name, Table{name, std::move(columns)});
}

void Database::dropTable(const std::string& name) {
    if (tables_.erase(name) == 0) throw std::runtime_error("Table does not exist: " + name);
}

Table& Database::table(const std::string& name) {
    auto it = tables_.find(name); if (it == tables_.end()) throw std::runtime_error("Table does not exist: " + name); return it->second;
}
const Table& Database::table(const std::string& name) const {
    auto it = tables_.find(name); if (it == tables_.end()) throw std::runtime_error("Table does not exist: " + name); return it->second;
}
bool Database::hasTable(const std::string& name) const { return tables_.count(name) != 0; }
std::vector<std::string> Database::tableNames() const { std::vector<std::string> r; for (const auto& [n, _] : tables_) r.push_back(n); return r; }
void Database::clear() { tables_.clear(); }

}
