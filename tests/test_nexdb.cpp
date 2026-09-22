#include "NexDB.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nexdb;

int main() {
    Database db;
    db.createTable("students", {{"id", DataType::Int}, {"name", DataType::Text}, {"marks", DataType::Int}});
    auto& t = db.table("students");
    t.insert({int64_t{1}, std::string{"Asha"}, int64_t{91}});
    t.insert({int64_t{2}, std::string{"Ravi"}, int64_t{78}});
    t.insert({int64_t{3}, std::string{"Maya"}, int64_t{95}});
    assert(t.select(std::nullopt).size() == 3);
    Condition c{"marks", Operator::Greater, Value{int64_t{90}}};
    assert(t.select(c).size() == 2);
    assert(t.update(c, 2, Value{int64_t{99}}) == 2);
    assert(t.remove(Condition{"id", Operator::Equal, Value{int64_t{2}}}) == 1);

    const auto dir = std::filesystem::temp_directory_path() / "nexdb_v1_test";
    std::filesystem::remove_all(dir);
    Storage storage(dir); storage.save(db);
    Database loaded; storage.load(loaded);
    assert(loaded.hasTable("students"));
    assert(loaded.table("students").rows().size() == 2);
    std::filesystem::remove_all(dir);

    Parser parser;
    assert(parser.parse("CREATE TABLE users (id INT, name TEXT);").columns.size() == 2);
    assert(parser.parse(".tables").type == ParsedCommand::Type::Tables);
    std::cout << "All NexDB tests passed.\n";
}
