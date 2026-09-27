// tests/native/test_ahp_format.cpp — Tests del parser ahp.v1

#include "doctest.h"
#include "fusion/data/AhpFormat.h"
#include <string>

using namespace fusion;

TEST_CASE("AhpFormat.CargarFromString válido") {
    const std::string json = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"Culto Test"},
        "escenarios":[{"id":"esc-1","nombre":"Bienvenida","elementos":[]}]
    })";
    Programa p;
    std::string err;
    bool ok = AhpFormat::CargarFromString(json, &p, &err);
    CHECK(ok);
    CHECK(p.titulo == "Culto Test");
    CHECK(p.escenarios.size() == 1);
    CHECK(p.escenarios[0].nombre == "Bienvenida");
}

TEST_CASE("AhpFormat.CargarFromString versión futura falla") {
    const std::string json = R"({
        "formato":"ahp","version":99,
        "meta":{},"escenarios":[]
    })";
    Programa p;
    std::string err;
    bool ok = AhpFormat::CargarFromString(json, &p, &err);
    CHECK(!ok);
    CHECK(!err.empty());
}

TEST_CASE("AhpFormat.Serializar luego CargarFromString redondea") {
    Programa p;
    p.titulo = "Ida y vuelta";
    Escenario e; e.id = "esc-1"; e.nombre = "Escena 1";
    Elemento el; el.id = "el-1"; el.tipo = TipoElemento::Texto;
    el.titulo = "Hola";
    el.lineas.push_back({"Hola mundo", "1"});
    e.elementos.push_back(el);
    p.escenarios.push_back(e);

    std::string s = AhpFormat::Serializar(p);
    Programa p2;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(s, &p2, &err));
    CHECK(p2.titulo == p.titulo);
    CHECK(p2.escenarios.size() == 1);
    CHECK(p2.escenarios[0].elementos.size() == 1);
    CHECK(p2.escenarios[0].elementos[0].lineas[0].texto == "Hola mundo");
}

TEST_CASE("AhpFormat.Validar detecta ids de escenario duplicados") {
    Programa p;
    Escenario e1; e1.id = "dup"; e1.nombre = "A";
    Escenario e2; e2.id = "dup"; e2.nombre = "B";
    p.escenarios.push_back(e1);
    p.escenarios.push_back(e2);
    std::string err;
    CHECK_FALSE(AhpFormat::Validar(p, &err));
}
