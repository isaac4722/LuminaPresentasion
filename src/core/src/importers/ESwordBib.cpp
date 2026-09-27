// src/core/src/importers/ESwordBib.cpp — Importador e-Sword .bib (Twofish)
//
// El cifrado Twofish en la capa gestionada usa BouncyCastle (net48).
// Aquí se expone el orquestador que delega al subproceso gestionado vía IPC
// si la operación se pide desde el núcleo. Por ahora: stub que documenta
// el flujo.

#include "fusion/importers/ESwordBib.h"
#include "fusion/data/BibleDatabase.h"

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

int ESwordBib::Importar(BibleDatabase& db, const std::string& ruta_bib) {
    std::ifstream f(ruta_bib, std::ios::binary);
    if (!f) return 0;

    // Cabecera de un .bib de e-Sword suele empezar con "wbible" o similar.
    char magic[8] = {0};
    f.read(magic, 6);
    if (std::string(magic, 6) != "wbible") {
        // Formato no reconocido en el stub. Documentar y devolver 0.
        return 0;
    }

    // TODO(P0): leer secciones, descifrar Twofish, parsear, insertar.
    (void)db;
    return 0;
}

bool ESwordBib::DescifrarTwofish(const std::string& entrada,
                                  std::string* salida,
                                  std::string* msg_error) {
    // La implementación real vive en la capa gestionada (BouncyCastle).
    // Aquí devolvemos false para indicar "no implementado en el núcleo".
    if (msg_error) *msg_error = "DescifrarTwofish se ejecuta en la capa gestionada";
    (void)entrada; (void)salida;
    return false;
}

} // namespace fusion
