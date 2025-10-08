#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace exoskeleton::core
{
    namespace commands
    {
        struct Connect {};
        auto operator <<(std::ostream& os, const Connect& c) -> std::ostream&;

        struct Disconnect {};
        auto operator <<(std::ostream& os, const Disconnect& c) -> std::ostream&;

        struct ConnectionStatus{};
        auto operator <<(std::ostream& os, const ConnectionStatus& c) -> std::ostream&;

        struct Enable
        {
            std::vector<std::string> names;
        };
        auto operator << (std::ostream& os, const Enable& c) -> std::ostream&;

        struct Disable
        {
            std::vector<std::string> names;
        };
        auto operator << (std::ostream& os, const Disable& c) -> std::ostream&;

        struct Function
        {
            std::vector<std::string> names;
            std::map<int, int> points;
        };
        auto operator << (std::ostream& os, const Function& c) -> std::ostream&;

        struct FunctionSelect
        {
            std::vector<std::string> names;
            int slot;
        };
        auto operator << (std::ostream& os, const FunctionSelect& c) -> std::ostream&;

        struct MultiFunctionSelect
        {
            std::map<std::string, int> slots;
        };
        auto operator << (std::ostream& os, const MultiFunctionSelect& c) -> std::ostream&;

        struct GetFunctions
        {
            std::vector<std::string> names;
        };
        auto operator << (std::ostream& os, const GetFunctions& c) -> std::ostream&;

        struct Zero
        {
            std::vector<std::string> names;
        };
        auto operator << (std::ostream& os, const Zero& c) -> std::ostream&;

        struct Offset
        {
            std::vector<std::string> names;
            int offset;
        };
        auto operator << (std::ostream& os, const Offset& c) -> std::ostream&;

        struct Stop {};
        auto operator << (std::ostream& os, const Stop& c) -> std::ostream&;

        struct Read {};
        auto operator << (std::ostream& os, const Read& c) -> std::ostream&;

        struct Exit {};
        auto operator << (std::ostream& os, const Exit& c) -> std::ostream&;

        struct SetEnv
        {
            std::string name;
            std::string value;
        };
        auto operator << (std::ostream& os, const SetEnv& c) -> std::ostream&;

        struct GetEnv {};
        auto operator << (std::ostream& os, const GetEnv& c) -> std::ostream&;

        struct SetPar
        {
            std::string name;
            std::string value;
        };
        auto operator << (std::ostream& os, const SetPar& c) -> std::ostream&;

        struct GetPar {};
        auto operator << (std::ostream& os, const GetPar& c) -> std::ostream&;

        struct ParEvent {};
        auto operator << (std::ostream& os, const ParEvent& c) -> std::ostream&;

    }

    using Command = std::variant<
        commands::Connect,
        commands::Disconnect,
        commands::ConnectionStatus,
        commands::Enable,
        commands::Disable,
        commands::Function,
        commands::FunctionSelect,
        commands::MultiFunctionSelect,
        commands::GetFunctions,
        commands::Zero,
        commands::Offset,
        commands::Stop,
        commands::Read,
        commands::Exit,
        commands::SetEnv,
        commands::GetEnv,
        commands::SetPar,
        commands::GetPar,
        commands::ParEvent
    >;

    auto parse(const std::string& command) -> std::optional<Command>;
}
