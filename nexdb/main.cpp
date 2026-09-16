#include "NexDB.hpp"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void enableGreenText() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    if (console != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;

        if (GetConsoleMode(console, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(console, mode);
        }
    }
#endif
}

void printLogo() {
    std::cout << "\033[1;32m";

    std::cout << R"(
███╗   ██╗███████╗██╗  ██╗██████╗ ██████╗
████╗  ██║██╔════╝╚██╗██╔╝██╔══██╗██╔══██╗
██╔██╗ ██║█████╗   ╚███╔╝ ██║  ██║██████╔╝
██║╚██╗██║██╔══╝   ██╔██╗ ██║  ██║██╔══██╗
██║ ╚████║███████╗██╔╝ ██╗██████╔╝██████╔╝
╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝╚═════╝ ╚═════╝
)";

    std::cout << "\033[0m";

    std::cout << "  Lightweight Database Engine • Modern C++\n";
    std::cout << "  Type .help for available commands.\n\n";
}

} // namespace

namespace nexdb {

std::string Engine::trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");

    if (a == std::string::npos)
        return {};

    const auto b = s.find_last_not_of(" \t\r\n");

    return s.substr(a, b - a + 1);
}

void Engine::printRows(
    const Table& table,
    const std::vector<std::size_t>& indices
) const {
    std::vector<std::size_t> widths;

    for (const auto& c : table.columns())
        widths.push_back(c.name.size());

    for (auto ri : indices) {
        for (std::size_t ci = 0; ci < widths.size(); ++ci) {
            widths[ci] = std::max(
                widths[ci],
                toString(table.valueAt(ri, ci)).size()
            );
        }
    }

    auto line = [&] {
        for (auto w : widths)
            std::cout << '+' << std::string(w + 2, '-');

        std::cout << "+\n";
    };

    line();

    for (std::size_t i = 0; i < widths.size(); ++i) {
        std::cout
            << "| "
            << std::left
            << std::setw(static_cast<int>(widths[i]))
            << table.columns()[i].name
            << ' ';
    }

    std::cout << "|\n";

    line();

    for (auto ri : indices) {
        for (std::size_t ci = 0; ci < widths.size(); ++ci) {
            std::cout
                << "| "
                << std::left
                << std::setw(static_cast<int>(widths[ci]))
                << toString(table.valueAt(ri, ci))
                << ' ';
        }

        std::cout << "|\n";
    }

    line();

    std::cout << indices.size() << " row(s)\n";
}

void Engine::printHelp() const {
    std::cout
        << "Commands:\n"
        << "  CREATE TABLE <name> (<col> <INT|DOUBLE|TEXT|BOOL>, ...);\n"
        << "  INSERT INTO <name> VALUES (<value>, ...);\n"
        << "  SELECT * FROM <name> [WHERE <col> <op> <value>];\n"
        << "  UPDATE <name> SET <col> = <value> WHERE <col> <op> <value>;\n"
        << "  DELETE FROM <name> [WHERE <col> <op> <value>];\n"
        << "  DROP TABLE <name>;\n"
        << "  .tables   .schema <name>   .save   .help   .exit\n";
}

void Engine::execute(
    const ParsedCommand& cmd,
    bool interactive
) {
    switch (cmd.type) {

        case ParsedCommand::Type::Create: {
            db_.createTable(cmd.table, cmd.columns);

            if (interactive)
                std::cout << "Table '" << cmd.table
                          << "' created.\n";

            break;
        }

        case ParsedCommand::Type::Drop: {
            db_.dropTable(cmd.table);

            if (interactive)
                std::cout << "Table dropped.\n";

            break;
        }

        case ParsedCommand::Type::Insert: {
            auto& t = db_.table(cmd.table);

            if (cmd.rawValues.size() != t.columns().size()) {
                throw std::runtime_error(
                    "Expected " +
                    std::to_string(t.columns().size()) +
                    " value(s)"
                );
            }

            Row row;

            for (std::size_t i = 0;
                 i < cmd.rawValues.size();
                 ++i) {

                row.push_back(
                    parseValue(
                        cmd.rawValues[i],
                        t.columns()[i].type
                    )
                );
            }

            t.insert(std::move(row));

            if (interactive)
                std::cout << "1 row inserted.\n";

            break;
        }

        case ParsedCommand::Type::Select: {
            auto& t = db_.table(cmd.table);

            auto cond = cmd.condition;

            if (cond) {
                const int idx =
                    t.columnIndex(cond->column);

                cond->value =
                    parseValue(
                        toString(cond->value),
                        t.columns()[idx].type
                    );
            }

            printRows(
                t,
                t.select(cond)
            );

            break;
        }

        case ParsedCommand::Type::Update: {
            auto& t = db_.table(cmd.table);

            const int target =
                t.columnIndex(cmd.updateColumn);

            const int condCol =
                t.columnIndex(cmd.condition->column);

            auto condition = *cmd.condition;

            condition.value =
                parseValue(
                    toString(condition.value),
                    t.columns()[condCol].type
                );

            const auto n =
                t.update(
                    condition,
                    static_cast<std::size_t>(target),
                    parseValue(
                        cmd.updateValue,
                        t.columns()[target].type
                    )
                );

            if (interactive)
                std::cout << n
                          << " row(s) updated.\n";

            break;
        }

        case ParsedCommand::Type::Delete: {
            auto& t = db_.table(cmd.table);

            auto cond = cmd.condition;

            if (cond) {
                const int idx =
                    t.columnIndex(cond->column);

                cond->value =
                    parseValue(
                        toString(cond->value),
                        t.columns()[idx].type
                    );
            }

            const auto n = t.remove(cond);

            if (interactive)
                std::cout << n
                          << " row(s) deleted.\n";

            break;
        }

        case ParsedCommand::Type::Tables: {
            for (const auto& n : db_.tableNames())
                std::cout << n << '\n';

            break;
        }

        case ParsedCommand::Type::Schema: {
            const auto& t =
                db_.table(cmd.table);

            std::cout
                << "Table: "
                << t.name()
                << '\n';

            for (const auto& c : t.columns()) {
                std::cout
                    << "  "
                    << c.name
                    << " "
                    << dataTypeToString(c.type)
                    << '\n';
            }

            break;
        }

        case ParsedCommand::Type::Help: {
            printHelp();
            break;
        }

        case ParsedCommand::Type::Save: {
            storage_.save(db_);

            std::cout
                << "Database saved to "
                << storage_.root()
                << ".\n";

            break;
        }

        case ParsedCommand::Type::Exit:
            return;

        default:
            throw std::runtime_error(
                "Nothing to execute"
            );
    }
}

void Engine::run() {

    try {
        storage_.load(db_);
    }
    catch (const std::exception& e) {
        std::cerr
            << "Load warning: "
            << e.what()
            << '\n';
    }

    // NexDB startup screen
    enableGreenText();
    printLogo();

    std::string line;

    while (
        std::cout << "nexdb> ",
        std::getline(std::cin, line)
    ) {

        line = trim(line);

        if (line.empty())
            continue;

        try {
            const auto cmd =
                parser_.parse(line);

            if (
                cmd.type ==
                ParsedCommand::Type::Exit
            ) {
                storage_.save(db_);
                break;
            }

            execute(cmd);
        }
        catch (const std::exception& e) {
            std::cerr
                << "Error: "
                << e.what()
                << '\n';
        }
    }
}

} // namespace nexdb

int main() {
    nexdb::Engine engine{"data"};

    engine.run();

    return 0;
}