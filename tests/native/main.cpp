// tests/native/main.cpp — Arnés doctest

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Cada test se incluye aquí. La compilación de fusion_tests.exe
// lleva los .cpp de los tests de cada pieza.
TEST_CASE("Arnés nativo: doctest arranca") {
    CHECK(1 + 1 == 2);
}
