// src/core/include/fusion/importers/ZefaniaXml.h
// Importador de biblias desde XML Zefania.

#pragma once

#include <string>

namespace fusion {

class BibleDatabase;

class ZefaniaXml {
public:
    // Importa una biblia Zefania XML a una BD de biblia ya abierta.
    // Devuelve número de versículos añadidos.
    static int Importar(BibleDatabase& db, const std::string& ruta_xml);
};

} // namespace fusion
