# How to use C++ First Class Exits

First class exits are std::variant exit types that allow you to specify all the success types and failure types that a given function can return. This is to support unit testing via static_assert().

This allows you:
- to have multiple different successful return types
- to have multiple different successful failure types
- to quickly determine if a return type is a success or a failure
- to assign each exit path its own unique failure type
- to have full exit path coverage in your unit test code
- to build and reuse a library of failure types for your project
- to have single-line exit handling (via "RETURN_IF(COND, CODE)")

This library includes a generic header that enables this, plus an example file showing a typical library of return types.

# Example client code

```
#include "FirstClassExits.hpp"

struct ParsedOk {
    unsigned int port;
    static constexpr std::string_view fmt_spec = "listening on port {}";
};

struct TextEmpty {
    static constexpr std::string_view fmt_spec = "port text was empty";
};

struct TooLong {
    std::size_t length;
    static constexpr std::string_view fmt_spec = "{} is too long for a port name";
};

struct BadSyntax {
    char badChar;
    std::size_t column;
    static constexpr std::string_view fmt_spec = "bad character '{}' at column {}";
};

struct IsZero {
    static constexpr std::string_view fmt_spec = "port should never be zero";
};

struct TooBig {
    unsigned int value;
    static constexpr std::string_view fmt_spec = "{} is too big for a port";
};

using PortExit = fce::Outcome<fce::Successes<ParsedOk>,
                              fce::Failures<TextEmpty, TooLong, BadSyntax, IsZero, TooBig>>;

inline constexpr PortExit parse_port_impl(std::string_view text) {
    constexpr std::size_t kMaxPortStringLength = 5;
    RETURN_IF(text.empty(), TextEmpty{});
    RETURN_IF(text.size() > kMaxPortStringLength, TooLong{.length = text.size()});

    unsigned int port = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        RETURN_IF(c < '0' || c > '9', BadSyntax{.badChar = c, .column = i + 1U});
        port = port * 10U + static_cast<unsigned int>(c - '0');
    }
    RETURN_IF(port == 0U, IsZero{});
    RETURN_IF(port >= 65536U, TooBig{port});
    return ParsedOk{.port = port};
}

PortExit parse_port(std::string_view text) {
    return parse_port_impl(text);
}

constexpr fce::Scenario<PortExit> port_scenarios[] = {
    fce::expect<ParsedOk>  ([] { return parse_port_impl("8080"); }),
    fce::expect<TextEmpty> ([] { return parse_port_impl(""); }),
    fce::expect<TooLong>   ([] { return parse_port_impl("123456"); }),
    fce::expect<BadSyntax> ([] { return parse_port_impl("80x0"); }),
    fce::expect<IsZero>    ([] { return parse_port_impl("0"); }),
    fce::expect<TooBig>    ([] { return parse_port_impl("99999"); }),
};
CHECK_SCENARIOS(port_scenarios);

static_assert( parse_port_impl("8080").get<ParsedOk>().port == 8080);
static_assert(!parse_port_impl("80x0"));
static_assert( parse_port_impl("80x0").get<BadSyntax>().badChar == 'x');
static_assert( parse_port_impl("80x0").get<BadSyntax>().column == 3);
static_assert( parse_port_impl("123456").get<TooLong>().length == 6);
static_assert( parse_port_impl("99999").get<TooBig>().value == 99999);

int main() {}

```
