#include "NexDB.hpp"

namespace nexdb {

Storage::Storage(std::filesystem::path root) : root_(std::move(root)) {}

void Storage::save(const Database& db) const {
    std::filesystem::create_directories(root_);
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (entry.path().extension() == ".nxt") std::filesystem::remove(entry.path());
    }
    for (const auto& name : db.tableNames()) {
        const auto& table = db.table(name);
        std::ofstream out(root_ / (name + ".nxt"), std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot open storage file for: " + name);
        out << "NEXDB1\n" << escape(table.name()) << '\n';
        out << table.columns().size();
        for (const auto& c : table.columns()) out << '|' << escape(c.name) << '|' << dataTypeToString(c.type);
        out << '\n';
        for (const auto& row : table.rows()) {
            for (std::size_t i = 0; i < row.size(); ++i) {
                if (i) out << '|';
                out << escape(toString(row[i]));
            }
            out << '\n';
        }
    }
}

namespace {
std::vector<std::string> splitEscaped(const std::string& line) {
    std::vector<std::string> result; std::string cur; bool escaped = false;
    for (char c : line) {
        if (escaped) { cur.push_back('\\'); cur.push_back(c); escaped = false; }
        else if (c == '\\') escaped = true;
        else if (c == '|') { result.push_back(unescape(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    if (escaped) cur.push_back('\\');
    result.push_back(unescape(cur));
    return result;
}
}

void Storage::load(Database& db) const {
    if (!std::filesystem::exists(root_)) return;
    db.clear();
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (entry.path().extension() != ".nxt") continue;
        std::ifstream in(entry.path()); if (!in) continue;
        std::string magic, name, schema; std::getline(in, magic); std::getline(in, name); std::getline(in, schema);
        if (magic != "NEXDB1") throw std::runtime_error("Invalid NexDB file: " + entry.path().string());
        auto meta = splitEscaped(schema); if (meta.empty()) throw std::runtime_error("Invalid schema file");
        const std::size_t count = static_cast<std::size_t>(std::stoul(meta[0]));
        if (meta.size() != 1 + count * 2) throw std::runtime_error("Corrupt schema: " + name);
        std::vector<Column> cols; for (std::size_t i = 0; i < count; ++i) cols.push_back({meta[1 + i * 2], dataTypeFromString(meta[2 + i * 2])});
        db.createTable(name, cols); std::vector<Row> rows; std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            auto fields = splitEscaped(line);
            if (fields.size() != cols.size()) throw std::runtime_error("Corrupt row in: " + name);
            Row row; for (std::size_t i = 0; i < cols.size(); ++i) row.push_back(parseValue(fields[i], cols[i].type)); rows.push_back(std::move(row));
        }
        db.table(name).loadRows(std::move(rows));
    }
}

}
