#include "NexDB.hpp"

namespace nexdb {

std::string Parser::trim(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string Parser::upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::vector<std::string> Parser::tokenize(const std::string& input) {
    std::vector<std::string> tokens; std::string cur; bool quote = false; char quoteChar = 0;
    auto flush = [&] { if (!cur.empty()) { tokens.push_back(cur); cur.clear(); } };
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (quote) {
            if (c == quoteChar) quote = false;
            else if (c == '\\' && i + 1 < input.size()) cur.push_back(input[++i]);
            else cur.push_back(c);
            continue;
        }
        if (c == '\'' || c == '"') { quote = true; quoteChar = c; continue; }
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == '(' || c == ')' || c == ';') { flush(); if (c == ',' || c == '(' || c == ')') tokens.emplace_back(1, c); }
        else if (c == '<' || c == '>' || c == '!' || c == '=') {
            flush(); std::string op(1, c); if (i + 1 < input.size() && input[i + 1] == '=') op += input[++i]; tokens.push_back(op);
        } else cur.push_back(c);
    }
    if (quote) throw std::runtime_error("Unterminated quoted string");
    flush(); return tokens;
}

Operator Parser::parseOperator(const std::string& token) {
    if (token == "=") return Operator::Equal;
    if (token == "!=") return Operator::NotEqual;
    if (token == "<") return Operator::Less;
    if (token == "<=") return Operator::LessEqual;
    if (token == ">") return Operator::Greater;
    if (token == ">=") return Operator::GreaterEqual;
    throw std::runtime_error("Invalid operator: " + token);
}

ParsedCommand Parser::parse(const std::string& input) const {
    auto tokens = tokenize(trim(input));
    if (tokens.empty()) return {};
    const auto cmd = upper(tokens[0]); ParsedCommand out;
    if (cmd == ".TABLES") { out.type = ParsedCommand::Type::Tables; return out; }
    if (cmd == ".HELP") { out.type = ParsedCommand::Type::Help; return out; }
    if (cmd == ".SAVE") { out.type = ParsedCommand::Type::Save; return out; }
    if (cmd == ".EXIT" || cmd == ".QUIT") { out.type = ParsedCommand::Type::Exit; return out; }
    if (cmd == ".SCHEMA") {
        if (tokens.size() != 2) throw std::runtime_error("Usage: .schema <table>");
        out.type = ParsedCommand::Type::Schema; out.table = tokens[1]; return out;
    }
    if (cmd == "DROP" && tokens.size() >= 3 && upper(tokens[1]) == "TABLE") {
        if (tokens.size() != 3) throw std::runtime_error("Usage: DROP TABLE <name>");
        out.type = ParsedCommand::Type::Drop; out.table = tokens[2]; return out;
    }
    if (cmd == "CREATE" && tokens.size() >= 4 && upper(tokens[1]) == "TABLE") {
        out.type = ParsedCommand::Type::Create; out.table = tokens[2];
        if (tokens[3] != "(") throw std::runtime_error("Expected '('");
        std::size_t i = 4;
        while (i < tokens.size() && tokens[i] != ")") {
            if (i + 1 >= tokens.size()) throw std::runtime_error("Incomplete column definition");
            out.columns.push_back({tokens[i], dataTypeFromString(tokens[i + 1])}); i += 2;
            if (i < tokens.size() && tokens[i] == ",") ++i;
        }
        if (i >= tokens.size() || tokens[i] != ")" || out.columns.empty()) throw std::runtime_error("Invalid CREATE TABLE syntax");
        return out;
    }
    if (cmd == "INSERT" && tokens.size() >= 5 && upper(tokens[1]) == "INTO") {
        out.type = ParsedCommand::Type::Insert; out.table = tokens[2];
        std::size_t i = 3;
        if (upper(tokens[i]) != "VALUES") { throw std::runtime_error("Expected VALUES"); }
        ++i;
        if (tokens[i++] != "(") throw std::runtime_error("Expected '('");
        while (i < tokens.size() && tokens[i] != ")") { if (tokens[i] != ",") out.rawValues.push_back(tokens[i]); ++i; }
        if (i >= tokens.size()) { throw std::runtime_error("Invalid INSERT syntax"); }
        return out;
    }
    if (cmd == "SELECT") {
        out.type = ParsedCommand::Type::Select;
        if (tokens.size() < 4 || tokens[1] != "*" || upper(tokens[2]) != "FROM") throw std::runtime_error("Only SELECT * FROM <table> is supported");
        out.table = tokens[3];
        if (tokens.size() > 4) {
            if (tokens.size() != 8 || upper(tokens[4]) != "WHERE") throw std::runtime_error("Usage: SELECT * FROM <table> WHERE <column> <op> <value>");
            out.condition = Condition{tokens[5], parseOperator(tokens[6]), {}}; out.condition->value = Value{tokens[7]};
        }
        return out;
    }
    if (cmd == "UPDATE") {
        if (tokens.size() < 8 || upper(tokens[2]) != "SET" || tokens[4] != "=" || upper(tokens[5]) != "WHERE") throw std::runtime_error("Usage: UPDATE <table> SET <column> = <value> WHERE <column> <op> <value>");
        out.type = ParsedCommand::Type::Update; out.table = tokens[1]; out.updateColumn = tokens[3]; out.updateValue = tokens[6];
        out.condition = Condition{tokens[6], parseOperator(tokens[7]), Value{tokens[8]}};
        if (tokens.size() != 9) { throw std::runtime_error("Invalid UPDATE condition"); }
        return out;
    }
    if (cmd == "DELETE" && tokens.size() >= 4 && upper(tokens[1]) == "FROM") {
        out.type = ParsedCommand::Type::Delete; out.table = tokens[2];
        if (tokens.size() == 7 && upper(tokens[3]) == "WHERE") out.condition = Condition{tokens[4], parseOperator(tokens[5]), Value{tokens[6]}};
        else if (tokens.size() != 3) throw std::runtime_error("Usage: DELETE FROM <table> [WHERE <column> <op> <value>]");
        return out;
    }
    throw std::runtime_error("Unknown command");
}

}
