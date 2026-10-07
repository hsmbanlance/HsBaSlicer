/** @file mock_dll_loader_dll.cpp
 * @brief A mock shared library used by dll_loader_test.
 *
 * Plain C functions exported for the Lua DllLoader binding (utils/LuaDllLoader.cpp)
 * to resolve and invoke at runtime. mock_answer/mock_pi/mock_nop are argument-free
 * (int / double / void returns), while mock_add/mock_scale take parameters so the
 * test can prove lua_dll_call_function now reads real argument values from the Lua
 * stack (previously it default-constructed the argument tuple and passed zeros).
 */
#ifdef _WIN32
#define MOCK_DL_EXPORT extern "C" __declspec(dllexport)
#else
#define MOCK_DL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

MOCK_DL_EXPORT int mock_answer(void)
{
    return 42;
}

MOCK_DL_EXPORT double mock_pi(void)
{
    // 3.5 is exactly representable in binary, so the round-trip comparison is stable.
    return 3.5;
}

MOCK_DL_EXPORT void mock_nop(void)
{
    // void return exercises the "no result pushed" branch of lua_dll_call_function.
}

MOCK_DL_EXPORT int mock_add(int a, int b)
{
    return a + b;
}

MOCK_DL_EXPORT double mock_scale(double a, double b)
{
    return a * b;
}
