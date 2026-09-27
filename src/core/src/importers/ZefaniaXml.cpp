// src/core/src/importers/ZefaniaXml.cpp — Importador Zefania XML

#include "fusion/importers/ZefaniaXml.h"
#include "fusion/data/BibleDatabase.h"

#include <fstream>
#include <sstream>
#include <string>
#include <regex>

namespace fusion {

int ZefaniaXml::Importar(BibleDatabase& db, const std::string& ruta_xml) {
    std::ifstream f(ruta_xml, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();

    // TODO(P0): parser XML real (pugixml o SAX de Windows IXMLDOMDocument).
    // Stub: cuenta <Verse> en el archivo para validar que la estructura
    // es reconocida.
    std::string contenido = ss.str();
    size_t n = 0;
    size_t pos = 0;
    while ((pos = contenido.find("<Verse", pos)) != std::string::npos) {
        ++n;
        pos += 6;
    }
    (void)db;
    return static_cast<int>(n);
}

} // namespace fusion
