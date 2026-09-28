// src/core/src/core/ExportadorPng.cpp — Codificador PNG y exportador de
// imágenes del núcleo (Sección 9.3).
//
// PNG válido y sin dependencias: IHDR RGBA 8 bits, IDAT con flujo zlib
// cuyo deflate usa SOLO bloques almacenados (RFC 1951 BTYPE=00) —
// comprimido en el sentido zlib, no en el de tamaño; cualquier visor y
// cualquier decodificador lo acepta. Los CRC de cada chunk y el adler32
// del flujo se calculan aquí, no se confían a nadie.
//
// El exportador de imágenes pide los píxeles de cada unidad al
// rasterizador del shell (Direct2D en Windows). El núcleo no rasteriza:
// define el punto de enganche y escribe el archivo.

#include "fusion/core/Exportador.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#ifdef __has_include
#if __has_include(<filesystem>)
#include <filesystem>
#define FUSION_TIENE_FS 1
#endif
#endif

#include "ZipInterno.h"  // Crc32 (el mismo polinomio de los chunks PNG)

namespace fusion {

namespace {

// Adler32 y ZlibAlmacenado viven en ZipInterno.h (fusion::zipint),
// compartidos con el PDF (FlateDecode) y el lector/escritor ZIP.
using fusion::zipint::ZlibAlmacenado;

void Poner32BE(std::string* s, uint32_t v) {
    s->push_back(static_cast<char>((v >> 24) & 0xFF));
    s->push_back(static_cast<char>((v >> 16) & 0xFF));
    s->push_back(static_cast<char>((v >> 8) & 0xFF));
    s->push_back(static_cast<char>(v & 0xFF));
}

void Chunk(std::string* out, const char tipo[4], const std::string& datos) {
    Poner32BE(out, static_cast<uint32_t>(datos.size()));
    out->append(tipo, 4);
    out->append(datos);
    std::string crc_in(tipo, tipo + 4);
    crc_in += datos;
    Poner32BE(out, zipint::Crc32(crc_in));
}

bool EscribirArchivo(const std::string& ruta, const std::string& bytes) {
    std::FILE* f = std::fopen(ruta.c_str(), "wb");
    if (!f) return false;
    const size_t n = bytes.size();
    const size_t escritos = n == 0 ? 0 : std::fwrite(bytes.data(), 1, n, f);
    std::fclose(f);
    return escritos == n;
}

} // namespace

std::string Exportador::CodificarPng(int ancho, int alto,
                                     const unsigned char* rgba) {
    if (ancho <= 0 || alto <= 0 || ancho > 16384 || alto > 16384 || !rgba)
        return "";
    const size_t fila = static_cast<size_t>(ancho) * 4;
    const size_t total = fila * static_cast<size_t>(alto);

    // IHDR: ancho, alto, profundidad 8, color 6 (RGBA), 0, 0, 0.
    std::string ihdr;
    Poner32BE(&ihdr, static_cast<uint32_t>(ancho));
    Poner32BE(&ihdr, static_cast<uint32_t>(alto));
    ihdr.push_back(static_cast<char>(8));   // profundidad
    ihdr.push_back(static_cast<char>(6));   // RGBA
    ihdr.push_back(static_cast<char>(0));   // compresión
    ihdr.push_back(static_cast<char>(0));   // filtro
    ihdr.push_back(static_cast<char>(0));   // entrelazado

    // Scanlines: byte de filtro 0 por fila + la fila RGBA.
    std::string crudo;
    crudo.reserve(total + alto);
    for (int y = 0; y < alto; ++y) {
        crudo.push_back(static_cast<char>(0));
        crudo.append(reinterpret_cast<const char*>(rgba + y * fila), fila);
    }

    std::string out;
    out += "\x89PNG\r\n\x1a\n";  // firma
    Chunk(&out, "IHDR", ihdr);
    Chunk(&out, "IDAT", ZlibAlmacenado(
           reinterpret_cast<const unsigned char*>(crudo.data()), crudo.size()));
    Chunk(&out, "IEND", "");
    return out;
}

bool Exportador::ExportarImagenes(const Programa& p,
                                  const std::string& dir_salida,
                                  const RasterizadorUnidad& rasterizar,
                                  ResultadoExport* out) {
    if (!out) return false;
    *out = ResultadoExport{};
    if (!rasterizar) {
        out->msg_error = "sin rasterizador del shell no hay imágenes";
        return false;
    }

    const PlanExportResultado plan =
        PlanExport::Planificar(p, FormatoExportacion::Imagenes1080p);
    if (!plan.ok) {
        out->msg_error = plan.msg_error;
        return false;
    }
    out->avisos.insert(out->avisos.end(), plan.avisos.begin(),
                       plan.avisos.end());

#ifdef FUSION_TIENE_FS
    std::error_code ec;
    std::filesystem::create_directories(dir_salida, ec);
    if (ec && !std::filesystem::exists(dir_salida)) {
        out->msg_error = "no se pudo crear la carpeta de salida '" +
                         dir_salida + "'";
        return false;
    }
#endif

    const std::vector<UnidadExport> unidades =
        PlanExport::EnumerarUnidades(p);

    std::string dir = dir_salida;
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
        dir.pop_back();
    if (dir.empty()) dir = ".";

    int con_medio = 0;
    for (size_t i = 0; i < unidades.size(); ++i) {
        if (unidades[i].tipo != UnidadExport::Tipo::Texto) ++con_medio;

        int ancho = 0, alto = 0;
        std::vector<unsigned char> rgba;
        if (!rasterizar(static_cast<int>(i) + 1, kAncho1080p, kAlto1080p,
                        &ancho, &alto, &rgba)) {
            out->msg_error = "el rasterizador falló en la unidad " +
                             std::to_string(i + 1);
            return false;
        }
        if (ancho <= 0 || alto <= 0 ||
            rgba.size() !=
                static_cast<size_t>(ancho) * static_cast<size_t>(alto) * 4) {
            out->msg_error = "el rasterizador devolvió un buffer incoherente "
                             "en la unidad " + std::to_string(i + 1);
            return false;
        }

        char nombre_archivo[32];
        std::snprintf(nombre_archivo, sizeof(nombre_archivo), "%02zu.png",
                      i + 1);
        const std::string ruta = dir + "/" + plan.nombre_base + "-" +
                                 nombre_archivo;
        const std::string png =
            CodificarPng(ancho, alto, rgba.data());
        if (png.empty() || !EscribirArchivo(ruta, png)) {
            out->msg_error = "no se pudo escribir '" + ruta + "'";
            return false;
        }
        out->archivos.push_back(ruta);
    }

    if (con_medio > 0) {
        out->avisos.push_back(std::to_string(con_medio) +
                              " unidad(es) de medio/pptx exportadas como "
                              "referencia textual (alcance v1)");
    }

    out->ok = true;
    return true;
}

} // namespace fusion
