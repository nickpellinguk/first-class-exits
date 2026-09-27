# first-class-exits
How to use First Class Exits in your C++ code

First class exits are std::variant exit types that allow you to quickly specify all the success types and failure types that a given function can return.

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

