// src/core/src/data/SemillaCantos.cpp — Himnos clásicos de dominio público
//
// Tradiciones hispanas de himnología del dominio público (letras
// tradicionales de himnos del siglo XIX y principios del XX, y cantos
// tradicionales anónimos). NADA con copyright vigente.

#include "fusion/data/SemillaCantos.h"

namespace fusion {
namespace semillacantos {

namespace {

CantoDetalle Base(const char* titulo, const char* autor, const char* tono,
                  int bpm) {
    CantoDetalle c;
    c.titulo      = titulo;
    c.autor       = autor;
    c.tono_origen = tono;
    c.bpm         = bpm;
    return c;
}

void Seccion(CantoDetalle* c, int orden, const char* tipo,
             const char* etiqueta, std::initializer_list<const char*> lineas) {
    CantoDetalle::Seccion s;
    s.orden    = orden;
    s.tipo     = tipo;
    s.etiqueta = etiqueta;
    for (const char* l : lineas) s.lineas.push_back(l);
    c->secciones.push_back(std::move(s));
}

} // namespace

std::vector<CantoDetalle> Generar() {
    std::vector<CantoDetalle> out;

    {   // 1
        auto c = Base("Santo, Santo, Santo", "Reginald Heber (trad.)", "C", 84);
        Seccion(&c, 1, "verso", "Verso 1", {
            "Santo, santo, santo, Señor omnipotente",
            "siempre el labio mío cantará tu honor.",
            "Santo, santo, santo, en terrón y en mar fuerte,",
            "toda la obra tuya proclama tu amor."});
        Seccion(&c, 2, "verso", "Verso 2", {
            "Santo, santo, santo, aunque ciego el hombre,",
            "tu gloria contempla la santa creación;",
            "y también mi canto te alza y te nombra:",
            "Dios en tres personas, santa Trinidad."});
        out.push_back(std::move(c));
    }
    {   // 2
        auto c = Base("A Dios Sea La Gloria", "Fanny Crosby (trad.)", "G", 76);
        Seccion(&c, 1, "verso", "Verso 1", {
            "A Dios sea la gloria por su gran amor,",
            "por dar a Su Hijo para redimir.",
            "En la cruz derramó Su sangre por mí,",
            "y abrióme la puerta del cielo entrar."});
        Seccion(&c, 2, "coro", "Coro", {
            "¡Oh, qué redención, gracia sobre gracia!",
            "El mundo verá la gran salvación;",
            "cuando venga el Rey en Su esplendor,",
            "a Dios sea la gloria por siempre, amén."});
        out.push_back(std::move(c));
    }
    {   // 3
        auto c = Base("Cristo Me Ama", "Anna Warner (trad.)", "F", 96);
        Seccion(&c, 1, "verso", "Verso 1", {
            "Cristo me ama, bien lo sé:",
            "su Palabra me dice así;",
            "son los niñitos de aquel Rey;",
            "yo su niño también soy."});
        Seccion(&c, 2, "coro", "Coro", {
            "Sí, Cristo me ama;",
            "sí, Cristo me ama;",
            "sí, Cristo me ama:",
            "la Biblia dice así."});
        out.push_back(std::move(c));
    }
    {   // 4
        auto c = Base("Castillo Fuerte", "Martín Lutero (trad.)", "D", 88);
        Seccion(&c, 1, "verso", "Verso 1", {
            "Castillo fuerte es nuestro Dios,",
            "espada y buen escudo;",
            "con su poder defiende nos,",
            "su nombre es de temido."});
        Seccion(&c, 2, "verso", "Verso 2", {
            "¿No es grande su fuerza? ¿Quién es él?",
            "el príncipe del mundo,",
            "más feroz y cruel,",
            "en la tierra sin igual."});
        out.push_back(std::move(c));
    }
    {   // 5
        auto c = Base("Alabad Al Señor", "Tradicional", "E", 100);
        Seccion(&c, 1, "coro", "Coro", {
            "Alabad al Señor, alabad al Señor,",
            "cantad a Él un cántico nuevo;",
            "alabad al Señor, alabad al Señor,",
            "por siglos de los siglos, amén."});
        out.push_back(std::move(c));
    }
    {   // 6
        auto c = Base("Cerca de Ti, Señor", "Sarah Adams (trad.)", "Bb", 72);
        Seccion(&c, 1, "verso", "Verso 1", {
            "Cerca de ti, Señor, más cerca de ti,",
            "aun cuando en tentación gima mi ser;",
            "ángel de consuelo, ven, acompáñame:",
            "cerca de ti, Señor, más cerca de ti."});
        out.push_back(std::move(c));
    }
    {   // 7
        auto c = Base("Cuán Dulce Es El Saber", "Fanny Crosby (trad.)", "A", 80);
        Seccion(&c, 1, "verso", "Verso 1", {
            "Cuán dulce es el saber que Jesús es mío,",
            "de su sangre lavado, redimido soy;",
            "y al pensar que me ama, gozo siento en mí,",
            "pues nací de nuevo y suya soy."});
        Seccion(&c, 2, "coro", "Coro", {
            "¡Jesús me salva! ¡Jesús me salva!",
            "¡Bendito sea el Señor por su amor!",
            "Cada día y hora me guarda y me sostiene:",
            "¡Jesús me salva por su gran amor!"});
        out.push_back(std::move(c));
    }
    {   // 8
        auto c = Base("Gloria, Gloria, Aleluya", "Tradicional", "G", 92);
        Seccion(&c, 1, "coro", "Coro", {
            "Gloria, gloria, aleluya;",
            "gloria, gloria, aleluya;",
            "gloria, gloria, aleluya;",
            "¡gloria a nuestro Rey!"});
        Seccion(&c, 2, "verso", "Verso 1", {
            "Él vive, Él vive, Cristo vive hoy,",
            "y a mi lado camina cada día;",
            "Él vive, Él vive, y le veré:",
            "¡gloria a nuestro Rey!"});
        out.push_back(std::move(c));
    }

    return out;
}

} // namespace semillacantos
} // namespace fusion
