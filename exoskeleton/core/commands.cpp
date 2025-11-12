#include "commands.h"

#include <lexy/dsl.hpp>
#include <lexy/callback.hpp>
#include <lexy/action/validate.hpp>
#include <lexy/action/parse_as_tree.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <iostream>
#include <utility>

namespace exoskeleton::core
{
    namespace
    {
        namespace grammar
        {
            namespace dsl = lexy::dsl;

            struct signed_int
            {
                static constexpr auto rule = dsl::sign + dsl::integer<int>;
                static constexpr auto value = lexy::as_integer<int>;
            };

            struct name
            {
                static constexpr auto rule
                    = dsl::identifier(dsl::unicode::alnum / dsl::lit_c<'_'>);
                static constexpr auto value = lexy::as_string<std::string>;
            };

            struct address_list {
                static constexpr auto rule = dsl::list(dsl::p<name>,dsl::sep(dsl::comma));
                static constexpr auto value = lexy::as_list<std::vector<std::string>>;
            };

            struct slot {
                static constexpr auto rule =  dsl::integer<int>;
                static constexpr auto value = lexy::forward<int>;
            };

            struct fpoint
            {
                static constexpr auto rule = dsl::integer<int> + dsl::lit_c<':'> + dsl::p<signed_int>;
                static constexpr auto value = lexy::construct<std::pair<int, int>>;
            };

            struct fpoint_list
            {
                static constexpr auto rule = dsl::no_whitespace(dsl::list(dsl::p<fpoint>, dsl::sep(dsl::ascii::space)));
                static constexpr auto value = lexy::as_collection<std::map<int, int>>;
            };

            struct c_command
            {
                static constexpr auto rule = LEXY_LIT("c");
                static constexpr auto value = lexy::constant(commands::Connect{});
            };

            struct dc_command
            {
                static constexpr auto rule = LEXY_LIT("dc");
                static constexpr auto value = lexy::constant(commands::Disconnect{});
            };

            struct cs_command
            {
                static constexpr auto rule = LEXY_LIT("cs");
                static constexpr auto value = lexy::constant(commands::ConnectionStatus{});
            };

            struct e_command
            {
                static constexpr auto rule = LEXY_LIT("e") >> dsl::p<address_list>;
                static constexpr auto value = lexy::construct<commands::Enable>;
            };

            struct d_command
            {
                static constexpr auto rule = LEXY_LIT("d") >> dsl::p<address_list>;
                static constexpr auto value = lexy::construct<commands::Disable>;
            };

            struct f_command
            {
                static constexpr auto rule = LEXY_LIT("f") >> dsl::p<address_list> + dsl::p<fpoint_list>;
                static constexpr auto value = lexy::construct<commands::Function>;
            };

            struct address_slot
            {
                static constexpr auto rule = dsl::p<name> + dsl::lit_c<':'> + dsl::integer<int>;
                static constexpr auto value = lexy::construct<std::pair<std::string, int>>;
            };

            struct address_slot_list
            {
                static constexpr auto rule = dsl::no_whitespace(dsl::list(dsl::p<address_slot>, dsl::sep(dsl::ascii::space)));
                static constexpr auto value = lexy::as_collection<std::map<std::string, int>>;
            };

            struct fs_command {
                static constexpr auto rule = LEXY_LIT("fs") >> dsl::p<address_list> + dsl::p<slot>;
                static constexpr auto value = lexy::construct<commands::FunctionSelect>;
            };

            struct ffs_command
            {
                static constexpr auto rule = LEXY_LIT("ffs") >> dsl::p<address_slot_list>;
                static constexpr auto value = lexy::construct<commands::MultiFunctionSelect>;
            };

            struct fg_command {
                static constexpr auto rule = LEXY_LIT("fg") >> dsl::p<address_list>;
                static constexpr auto value = lexy::construct<commands::GetFunctions>;
            };

            struct z_command
            {
                static constexpr auto rule = LEXY_LIT("z") >> dsl::p<address_list>;
                static constexpr auto value = lexy::construct<commands::Zero>;
            };

            struct op_command
            {
                static constexpr auto rule = LEXY_LIT("op") >> dsl::p<address_list> + dsl::p<signed_int>;
                static constexpr auto value = lexy::construct<commands::Offset>;
            };

            struct s_command {
                static constexpr auto rule = LEXY_LIT("s");
                static constexpr auto value = lexy::constant(commands::Stop{});
            };

            struct r_command
            {
                static constexpr auto rule = LEXY_LIT("r");
                static constexpr auto value = lexy::constant(commands::Read{});
            };

            struct x_command
            {
                static constexpr auto rule = LEXY_LIT("x");
                static constexpr auto value = lexy::constant(commands::Exit{});
            };

            struct config_value
            {
                static constexpr auto rule = dsl::list(dsl::capture(dsl::code_point));
                static constexpr auto value = lexy::as_string<std::string>;
            };

            struct env_command
            {
                static constexpr auto rule = LEXY_LIT("env") >> dsl::p<name> + dsl::p<config_value>;
                static constexpr auto value = lexy::construct<commands::SetEnv>;
            };

            struct production
            {
                // Allow arbitrary spaces between individual tokens.
                static constexpr auto whitespace = dsl::ascii::space;
                static constexpr auto rule = [] {
                    return (
                        dsl::p<cs_command>
                        | dsl::p<c_command>
                        | dsl::p<env_command>
                        | dsl::p<e_command>
                        | dsl::p<dc_command>
                        | dsl::p<d_command>
                        | dsl::p<fs_command>
                        | dsl::p<ffs_command>
                        | dsl::p<fg_command>
                        | dsl::p<f_command>
                        | dsl::p<s_command>
                        | dsl::p<z_command>
                        | dsl::p<op_command>
                        | dsl::p<r_command>
                        | dsl::p<x_command>
                        ) + dsl::eof;
                }();
                static constexpr auto value = lexy::construct<Command>;
            };
        }
    }

    namespace commands
    {
        template<typename T> auto operator <<(std::ostream& os, const std::vector<T>& v) -> std::ostream&
        {
            os << "[ ";
            for (const auto& e : v)
            {
                os << e << ' ';
            }
            os << ']';
            return os;
        }

        template<typename K, typename V> auto operator <<(std::ostream& os, const std::map<K, V>& v) -> std::ostream&
        {
            os << "{ ";
            for (const auto& [key, value] : v)
            {
                os << key << ":" << value << " ";
            }
            os << '}';
            return os;
        }

        auto operator <<(std::ostream& os, const Connect& c) -> std::ostream&
        {
            return os << "Connect()";
        }

        auto operator <<(std::ostream& os, const Disconnect& c) -> std::ostream&
        {
            return os << "Disconnect()";
        }

        auto operator<<(std::ostream& os, const ConnectionStatus& c) -> std::ostream&
        {
            return os << "ConnectionStatus()";
        }

        auto operator << (std::ostream& os, const Enable& c) -> std::ostream&
        {
            return os << "Enable(" << c.names << ')';
        }

        auto operator << (std::ostream& os, const Disable& c) -> std::ostream&
        {
            return os << "Disable(" << c.names << ')';
        }

        auto operator << (std::ostream& os, const Function& c) -> std::ostream&
        {
            return os << "Function(" << c.names << ", " << c.points << ')';
        }

        auto operator << (std::ostream& os, const FunctionSelect& c) -> std::ostream&
        {
            return os << "FunctionSelect(" << c.names << " ," << c.slot << ")";
        }

        auto operator << (std::ostream& os, const MultiFunctionSelect& c) -> std::ostream&
        {
            return os << "MultiFunctionSelect(" << c.slots << ')';
        }

        auto operator<<(std::ostream& os, const GetFunctions& c) -> std::ostream&
        {
            return os <<"GetFunctions(" << c.names << ')';
        }

        auto operator << (std::ostream& os, const Zero& c) -> std::ostream&
        {
            return os << "Zero(" << c.names << ')';
        }

        auto operator<<(std::ostream& os, const Offset& c) -> std::ostream&
        {
            return os << "Offset(" << c.names << ", " << c.offset << ')';
        }

        auto operator << (std::ostream& os, const Stop&) -> std::ostream&
        {
            return os << "Stop()";
        }

        auto operator << (std::ostream& os, const Read& c) -> std::ostream&
        {
            return os << "Read()";
        }

        auto operator<<(std::ostream& os, const Exit& c) -> std::ostream&
        {
            return os << "Exit()";
        }

        auto operator<<(std::ostream& os, const SetEnv& c) -> std::ostream&
        {
            return os << "SetEnv(" << c.name << ", " << c.value << ")";
        }

        auto operator<<(std::ostream& os, const GetEnv& c) -> std::ostream&
        {
            return os << "GetEnv()";
        }

        auto operator<<(std::ostream& os, const SetPar& c) -> std::ostream&
        {
            return os << "SetPar(" << c.name << ", " << c.value << ")";
        }

        auto operator<<(std::ostream& os, const GetPar& c) -> std::ostream&
        {
            return os << "GetPar()";
        }

        auto operator<<(std::ostream& os, const ParEvent& c) -> std::ostream&
        {
            return os << "ParEvent()";
        }

    }

    auto parse(const std::string& command) -> std::optional<Command>
    {
        auto input = lexy::string_input<lexy::utf8_encoding>{command};

        //lexy::parse_tree_for<decltype(input)> tree;
        //auto result2 = lexy::parse_as_tree<grammar::production>(tree, input, lexy_ext::report_error);
        //std::cout << result2.is_success() << std::endl;
        //lexy::visualize(stdout, tree);
        auto result3 = lexy::parse<grammar::production>(input, lexy_ext::report_error);
        if (result3.has_value())
        {
            std::visit([](const auto& e){ std::cout << e; }, result3.value());
            std::cout << std::endl;
            return result3.value();
        }
        return {};
    }
}
