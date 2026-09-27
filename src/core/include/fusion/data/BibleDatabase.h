// src/core/include/fusion/data/BibleDatabase.h
// Una biblia por .fdb (esquema data/schema/bible.sql).
// 4 biblias preinstaladas en data/bibles/.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fusion {

struct LibroBiblia {
    int          id;             // 1..66
    std::string  nombre;
    std::string  abrev3;
    std::string  abrev2;
    std::string  testamento;     // "AT" / "NT"
};

struct Versiculo {
    std::string libro;
    int         capitulo;
    int         versiculo;
    std::string texto;
};

class BibleDatabase {
public:
    BibleDatabase();
    ~BibleDatabase();

    bool Abrir(const std::string& ruta_fdb);
    void Cerrar();

    // Árbol de 66 libros siempre visible.
    std::vector<LibroBiblia> ListarLibros() const;

    // Capítulos de un libro (para llenar la vista de árbol).
    std::vector<int> ListarCapitulos(int libro_id) const;

    // Versículos de un capítulo.
    std::vector<Versiculo> ObtenerCapitulo(int libro_id, int capitulo) const;

    // Cita directa "Salmo 100:4" o "Juan 3:16-18".
    bool ObtenerCita(const std::string& cita, std::vector<Versiculo>* out) const;

    // Búsqueda libre (FTS5). ≤200 ms objetivo P2.
    std::vector<Versiculo> Buscar(const std::string& texto, int limite = 200) const;

    // Favoritos del operador (lista plana).
    std::vector<std::string> Favoritos() const;
    bool AgregarFavorito(const std::string& cita);
    bool QuitarFavorito(const std::string& cita);

    // Importadores (devuelven número de versículos importados).
    int ImportarZefaniaXml(const std::string& ruta_xml);
    int ImportarESwordBib(const std::string& ruta_bib);
    int ImportarJson(const std::string& ruta_json);
    int ImportarTsv(const std::string& ruta_tsv);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
