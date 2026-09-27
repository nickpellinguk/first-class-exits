#pragma once
// ============================================================================
// FirstClassExits.hpp
//
// First-class exits: every way that a function can exit is given its own exit
// type declared in the function's contract, and can then be proven at compile time.
//
// An individual exit is a plain struct, containing its data members, then a
//     static constexpr std::string_view fmt_spec
// whose {} placeholders are filled by the data members in declaration order.
// A struct may instead supply fields() returning a std::tie of the members to
// render - this allows you select or reorder them as part of the format.
// fmt_spec uses std::format syntax, so placeholders may carry format specs,
// e.g. {:04} or {:x}, and {{ }} produce literal braces.
//
// The exit contract is fce::Outcome<fce::Successes<...>, fce::Failures<...>>.
// Classification is defined by whether a type is in the Successes<> type list
// or in the Failures<> type list.
//
// A function returns exits by value, directly or conveniently using RETURN_IF()
//
// Unit tests user Scenarios to prove the contract: an fce::Scenario table holding
// fce::expect<Exit>(captureless lambda) entries is then checked by CHECK_SCENARIOS.
//
// The compiler refuses:
// - an exit with no scenario
// - a scenario reaching the wrong exit
// - a missing fmt_spec
// - a fmt_spec whose placeholder count doesn't match its fields
// - a type listed twice
// - a contract with no success
// - the lists in the wrong order
// - a return of an undeclared exit
// - an ignored result
//
// Requires C++20 (for std::format).
// ============================================================================

#include <concepts>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace fce {

// ----------------------------------------------------------------

// One typelist contains all the success types, the other all the failure types
template<class...> struct Successes {};
template<class...> struct Failures {};

// Hide the implementation details from general view
namespace detail {
    struct any_field {
        template<class T> constexpr operator T() const;
    };

    // Count an aggregate's data members by trying ever-longer brace initialisations.
    template<class T, class... A>
    constexpr std::size_t field_count() {
        if constexpr (requires { T{A{}..., any_field{}}; })
            return field_count<T, A..., any_field>();
        else
            return sizeof...(A);
    }

    template<class T>
    constexpr auto fields_of(const T& t) {
        if constexpr (requires { t.fields(); }) {
            return t.fields();
        } else {
            constexpr auto n = field_count<T>();
            if constexpr (n == 0)
                return std::tuple<>{};
            else if constexpr (n == 1) { const auto& [a] = t;
                return std::tie(a); }
            else if constexpr (n == 2) { const auto& [a, b] = t;
                return std::tie(a, b); }
            else if constexpr (n == 3) { const auto& [a, b, c] = t;
                return std::tie(a, b, c); }
            else if constexpr (n == 4) { const auto& [a, b, c, d] = t;
                return std::tie(a, b, c, d); }
            else if constexpr (n == 5) { const auto& [a, b, c, d, e] = t;
                return std::tie(a, b, c, d, e); }
            else if constexpr (n == 6) { const auto& [a, b, c, d, e, f] = t;
                return std::tie(a, b, c, d, e, f); }
            else if constexpr (n == 7) { const auto& [a, b, c, d, e, f, g] = t;
                return std::tie(a, b, c, d, e, f, g); }
            else
                static_assert(n <= 7, "more than 7 fields: give this exit a fields() member");
        }
    }

    template<class T>
    using fields_t = decltype(fields_of(std::declval<const T&>()));

    // Counts replacement fields: {} or {:spec}, skipping escaped {{ braces.
    constexpr std::size_t placeholder_count(std::string_view s) {
        std::size_t n = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] != '{')
                continue;
            if (i + 1 < s.size() && s[i + 1] == '{') {
                ++i;
                continue;
            }
            ++n;
        }
        return n;
    }

    template<class T, class... Ts>
    inline constexpr std::size_t count_of = (std::size_t{0} + ... + std::is_same_v<T, Ts>);

    template<class T, class... Ts>
    constexpr std::size_t index_in() {
        constexpr bool matches[] = { std::is_same_v<T, Ts>... };
        for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
            if (matches[i])
                return i;
        }
        return sizeof...(Ts);
    }
}

template<class P>
concept HasFmtSpec = requires { { P::fmt_spec } -> std::convertible_to<std::string_view>; };

// An exit must have a fmt_spec field, and its placeholder count should matches its field count
template<class P>
concept Exit = HasFmtSpec<P> &&
               detail::placeholder_count(P::fmt_spec) == std::tuple_size_v<detail::fields_t<P>>;

// ----------------------------------------------------------------

// std::format checks fmt_spec against the field types at compile time.
template<Exit P>
std::string render(const P& exit) {
    return std::apply([](const auto&... fields) {
        return std::format(P::fmt_spec, fields...);
    }, detail::fields_of(exit));
}

// ----------------------------------------------------------------

template<class S, class F>
struct Outcome {
    static_assert(sizeof(S) == 0, "Outcome takes fce::Successes<...> then fce::Failures<...>");
};

template<class... S, class... F>
struct [[nodiscard]] Outcome<Successes<S...>, Failures<F...>> : std::variant<S..., F...> {
    using variant_type = std::variant<S..., F...>;

    static_assert(sizeof...(S) + sizeof...(F)> 0,
                  "every exit contract must have at least one exit type");
    static_assert(((detail::count_of<S, S..., F...> == 1) && ...) &&
                  ((detail::count_of<F, S..., F...> == 1) && ...),
                  "each exit type may appear only once across both lists");
    static_assert((HasFmtSpec<S> && ...) && (HasFmtSpec<F> && ...),
                  "every exit needs: static constexpr std::string_view fmt_spec");
    static_assert((Exit<S> && ...) && (Exit<F> && ...),
                  "every exit's fmt_spec needs one placeholder per field");

    using variant_type::variant_type;

    static constexpr std::size_t size = sizeof...(S) + sizeof...(F);

    template<class T>
        requires (detail::count_of<T, S..., F...> == 1)
    static constexpr std::size_t index_of() { return detail::index_in<T, S..., F...>(); }

    constexpr const variant_type& as_variant() const { return *this; }

    // Successes come first in the variant, so classification is a simple comparison.
    constexpr bool succeeded() const { return this->index() < sizeof...(S); }
    constexpr explicit operator bool() const { return succeeded(); }

    template<class T> constexpr bool is() const { return std::holds_alternative<T>(as_variant()); }
    template<class T> constexpr const T& get() const { return std::get<T>(as_variant()); }

    // Runtime only: std::format is not constexpr.
    std::string message() const {
        return std::visit([](const auto& exit) { return render(exit); }, as_variant());
    }
};

// ----------------------------------------------------------------
// A Scenario is an expected exit plus a captureless lambda producing an
// outcome. The lambda may do any constexpr setup before returning it.

template<class O>
struct Scenario {
    std::size_t exit;
    O (*run)();
};

template<class T, class F>
constexpr auto expect(F lambda) {
    using O = std::invoke_result_t<F>;
    static_assert(std::is_convertible_v<F, O (*)()>,
                  "scenario lambdas must not capture: put their setup inside the body");
    return Scenario<O>{O::template index_of<T>(), lambda};
}

template<class O, std::size_t N>
constexpr bool covers_every_exit(const Scenario<O> (&rows)[N]) {
    bool seen[O::size] = {};
    for (const auto& r : rows) {
        seen[r.exit] = true;
    }
    for (bool s : seen) {
        if (!s)
            return false;
    }
    return true;
}

template<class O, std::size_t N>
constexpr bool every_scenario_reaches_its_exit(const Scenario<O> (&rows)[N]) {
    for (const auto& r : rows) {
        if (r.run().index() != r.exit)
            return false;
    }
    return true;
}

} // namespace fce

// ============================================================================
// Macros
// ============================================================================

// Prove that every exit has a scenario, and that every scenario yield its declared exit.
#define FCE_CHECK_SCENARIOS(TABLE)                               \
    static_assert(::fce::covers_every_exit(TABLE),               \
                  #TABLE ": an exit has no scenario");           \
    static_assert(::fce::every_scenario_reaches_its_exit(TABLE), \
                  #TABLE ": a scenario reached the wrong exit")

// Returns the given exit when the condition holds. The exit is variadic because
// the preprocessor splits arguments at commas, including those inside braces.
#define FCE_RETURN_IF(CONDITION, ...) \
    do { \
        if (CONDITION) \
            return __VA_ARGS__; \
    } while (false)

// Short name, unless a project opts out to avoid collisions.
#ifndef FCE_NO_SHORT_MACROS
#define RETURN_IF(CONDITION, ...)  FCE_RETURN_IF(CONDITION, __VA_ARGS__)
#define CHECK_SCENARIOS(TABLE)     FCE_CHECK_SCENARIOS(TABLE)
#endif
