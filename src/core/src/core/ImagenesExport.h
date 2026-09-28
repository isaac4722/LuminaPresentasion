// src/core/src/core/ImagenesExport.h — Ayudas de imagen compartidas por
// los exportadores (9.3): clasificación por extensión, lectura binaria,
// decodificador PNG y dimensiones JPEG. Interno (no es API pública).
//
// Todo propio y portable (el inflador es el de ZipInterno.h): cero
// dependencias nuevas, 100 % offline. Lo no soportado se reporta como
// no-ok con mensaje — nada silencioso.

#pragma once

#include "ZipInterno.h"  // Inflar, Crc32

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fusion::imgexp {

// ---------------------------------------------------------------------------
// Clasificación por extensión (case-insensitive)
// ---------------------------------------------------------------------------

enum class TipoImagen { Ninguna, Png, Jpeg };

// Lectura BIG-ENDIAN (PNG guarda longitudes, CRC e IHDR en orden de red;
// el Leer32 de ZipInterno es little-endian por el formato ZIP).
inline uint32_t Leer32BE(const unsigned char* b, size_t tam, size_t pos) {
    if (pos + 4 > tam) return 0;
    return (static_cast<uint32_t>(b[pos]) << 24) |
           (static_cast<uint32_t>(b[pos + 1]) << 16) |
           (static_cast<uint32_t>(b[pos + 2]) << 8) |
           static_cast<uint32_t>(b[pos + 3]);
}

inline bool TerminaCon(const std::string& s, const std::string& sufijo) {
    if (s.size() < sufijo.size()) return false;
    for (size_t i = 0; i < sufijo.size(); ++i) {
        char a = s[s.size() - sufijo.size() + i];
        char b = sufijo[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

inline TipoImagen TipoPorRuta(const std::string& ruta) {
    if (TerminaCon(ruta, ".png")) return TipoImagen::Png;
    if (TerminaCon(ruta, ".jpg")) return TipoImagen::Jpeg;
    if (TerminaCon(ruta, ".jpeg")) return TipoImagen::Jpeg;
    return TipoImagen::Ninguna;
}

// ---------------------------------------------------------------------------
// Lectura binaria
// ---------------------------------------------------------------------------

inline bool LeerArchivo(const std::string& ruta, std::string* bytes) {
    if (!bytes) return false;
    bytes->clear();
    std::FILE* f = std::fopen(ruta.c_str(), "rb");
    if (!f) return false;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        bytes->append(buf, n);
    const bool ok = std::ferror(f) == 0 && !bytes->empty();
    std::fclose(f);
    return ok;
}

// ---------------------------------------------------------------------------
// Colores: "#RGB"/"#RRGGBB" → "RRGGBB" (para srgbClr del PPTX y para
// el `rg` del PDF). Vacío si el color no es válido.
// ---------------------------------------------------------------------------

inline std::string FondoHex6(const std::string& hex) {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    auto hex_ok = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    };
    if (s.size() == 3) {
        for (char c : s)
            if (!hex_ok(c)) return std::string();
        std::string dup;
        for (char c : s) {
            dup.push_back(c);
            dup.push_back(c);
        }
        return dup;
    }
    if (s.size() != 6) return std::string();
    for (char c : s)
        if (!hex_ok(c)) return std::string();
    return s;
}

// ---------------------------------------------------------------------------
// Decodificador PNG (v1): profundidad 8, colourtypes 0 (gris), 2 (RGB) y
// 6 (RGBA, alfa aplanada sobre blanco), sin entrelazado. Paleta (3) y
// entrelazado Adam7 → no soportados con mensaje explícito.
// ---------------------------------------------------------------------------

struct PngDecodificado {
    bool        ok = false;
    std::string msg;          // vacío si ok
    int         ancho = 0;
    int         alto = 0;
    std::string rgb;          // ancho*alto*3 bytes, filas de arriba abajo
};

inline int Paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

inline PngDecodificado DecodificarPng(const std::string& bytes) {
    PngDecodificado r;
    const unsigned char* d = reinterpret_cast<const unsigned char*>(bytes.data());
    const size_t n = bytes.size();
    static const unsigned char kFirma[8] = {0x89, 'P', 'N', 'G', '\r', '\n',
                                            0x1A, '\n'};
    if (n < 8 || std::memcmp(d, kFirma, 8) != 0) {
        r.msg = "no es un PNG (firma inválida)";
        return r;
    }
    size_t pos = 8;
    int ancho = 0, alto = 0, profundidad = 0, color = 0, entrelazado = 0;
    std::string idat;
    bool ihdr_visto = false;
    while (pos + 8 <= n) {
        const uint32_t lon = Leer32BE(d, n, pos);
        if (pos + 12 + static_cast<size_t>(lon) > n) {
            r.msg = "chunk PNG truncado";
            return r;
        }
        const char* tipo = reinterpret_cast<const char*>(d + pos + 4);
        const unsigned char* dato = d + pos + 8;
        // Verificación CRC del chunk (no se confía en nadie).
        std::string crc_in(tipo, tipo + 4);
        crc_in.append(reinterpret_cast<const char*>(dato), lon);
        if (zipint::Crc32(crc_in) != Leer32BE(d, n, pos + 8 + lon)) {
            r.msg = "chunk PNG con CRC inválido";
            return r;
        }
        if (std::memcmp(tipo, "IHDR", 4) == 0) {
            if (lon < 13) { r.msg = "IHDR truncado"; return r; }
            ancho = static_cast<int>(Leer32BE(dato, lon, 0));
            alto = static_cast<int>(Leer32BE(dato, lon, 4));
            profundidad = dato[8];
            color = dato[9];
            entrelazado = dato[12];
            ihdr_visto = true;
        } else if (std::memcmp(tipo, "IDAT", 4) == 0) {
            idat.append(reinterpret_cast<const char*>(dato), lon);
        } else if (std::memcmp(tipo, "IEND", 4) == 0) {
            pos += 12 + lon;
            break;
        }
        pos += 12 + lon;
    }
    if (!ihdr_visto) { r.msg = "PNG sin IHDR"; return r; }
    if (ancho <= 0 || alto <= 0 || ancho > 16384 || alto > 16384) {
        r.msg = "dimensiones PNG fuera de rango";
        return r;
    }
    if (profundidad != 8) {
        r.msg = "profundidad PNG no soportada (solo 8 bits): " +
                std::to_string(profundidad);
        return r;
    }
    if (entrelazado != 0) {
        r.msg = "PNG entrelazado (Adam7) no soportado";
        return r;
    }
    int canales = 0;
    if (color == 0) canales = 1;       // gris
    else if (color == 2) canales = 3;  // RGB
    else if (color == 6) canales = 4;  // RGBA
    else {
        r.msg = "tipo de color PNG no soportado (solo gris/RGB/RGBA)";
        return r;
    }
    if (idat.size() < 2) { r.msg = "PNG sin datos IDAT"; return r; }

    // Inflar el contenido (sin la cabecera zlib de 2 bytes; el adler del
    // final no participa en el deflate crudo).
    const size_t fila = static_cast<size_t>(ancho) * canales;
    const size_t crudo_esperado = (fila + 1) * static_cast<size_t>(alto);
    std::vector<unsigned char> crudo;
    if (!zipint::Inflar(reinterpret_cast<const unsigned char*>(idat.data()) + 2,
                        idat.size() - 2, crudo_esperado, crudo)) {
        r.msg = "datos PNG corruptos (deflate inválido o tamaño inesperado)";
        return r;
    }

    // Des-filtrar: cada fila lleva su byte de filtro (0..4).
    r.rgb.resize(static_cast<size_t>(ancho) * alto * 3);
    std::vector<unsigned char> previa(fila, 0), actual(fila, 0);
    for (int y = 0; y < alto; ++y) {
        const unsigned char* fila_in = crudo.data() +
            static_cast<size_t>(y) * (fila + 1);
        const int filtro = fila_in[0];
        const unsigned char* cruda = fila_in + 1;
        std::copy(cruda, cruda + fila, actual.begin());
        switch (filtro) {
            case 0: break;  // Ninguno
            case 1:  // Sub
                for (size_t i = static_cast<size_t>(canales); i < fila; ++i)
                    actual[i] = static_cast<unsigned char>(
                        actual[i] + actual[i - canales]);
                break;
            case 2:  // Up
                for (size_t i = 0; i < fila; ++i)
                    actual[i] = static_cast<unsigned char>(
                        actual[i] + previa[i]);
                break;
            case 3:  // Average
                for (size_t i = 0; i < fila; ++i) {
                    const int izq = i >= static_cast<size_t>(canales)
                                        ? actual[i - canales] : 0;
                    const int arriba = previa[i];
                    actual[i] = static_cast<unsigned char>(
                        actual[i] + (izq + arriba) / 2);
                }
                break;
            case 4:  // Paeth
                for (size_t i = 0; i < fila; ++i) {
                    const int izq = i >= static_cast<size_t>(canales)
                                        ? actual[i - canales] : 0;
                    const int arriba = previa[i];
                    const int diag = i >= static_cast<size_t>(canales)
                                         ? previa[i - canales] : 0;
                    actual[i] = static_cast<unsigned char>(
                        actual[i] + Paeth(izq, arriba, diag));
                }
                break;
            default:
                r.msg = "filtro PNG desconocido: " + std::to_string(filtro);
                return r;
        }
        // Convertir a RGB (alfa aplanada sobre blanco).
        unsigned char* fila_out = reinterpret_cast<unsigned char*>(&r.rgb[0]) +
                                  static_cast<size_t>(y) * ancho * 3;
        for (int x = 0; x < ancho; ++x) {
            const unsigned char* px = &actual[static_cast<size_t>(x) * canales];
            unsigned char rr, gg, bb;
            if (color == 0) {
                rr = gg = bb = px[0];
            } else if (color == 2) {
                rr = px[0]; gg = px[1]; bb = px[2];
            } else {
                const int a = px[3];
                const int inv = 255 - a;
                rr = static_cast<unsigned char>(
                    (px[0] * a + 255 * inv + 127) / 255);
                gg = static_cast<unsigned char>(
                    (px[1] * a + 255 * inv + 127) / 255);
                bb = static_cast<unsigned char>(
                    (px[2] * a + 255 * inv + 127) / 255);
            }
            fila_out[static_cast<size_t>(x) * 3 + 0] = rr;
            fila_out[static_cast<size_t>(x) * 3 + 1] = gg;
            fila_out[static_cast<size_t>(x) * 3 + 2] = bb;
        }
        previa = actual;
    }
    r.ok = true;
    r.ancho = ancho;
    r.alto = alto;
    return r;
}

// ---------------------------------------------------------------------------
// Dimensiones JPEG: escaneo de marcadores hasta el SOF (C0-CF salvo
// C4/DHT, C8/JPG y CC/DAC). Suficiente para el diccionario del XObject.
// ---------------------------------------------------------------------------

inline bool DimensionesJpeg(const std::string& bytes, int* ancho, int* alto) {
    const unsigned char* d = reinterpret_cast<const unsigned char*>(bytes.data());
    const size_t n = bytes.size();
    if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;  // SOI
    size_t pos = 2;
    while (pos + 4 <= n) {
        if (d[pos] != 0xFF) return false;
        // Relleno de 0xFF antes del marcador.
        while (pos < n && d[pos] == 0xFF) ++pos;
        if (pos >= n) return false;
        const unsigned char marcador = d[pos++];
        if (marcador == 0x01 || (marcador >= 0xD0 && marcador <= 0xD7))
            continue;  // sin longitud
        if (pos + 2 > n) return false;
        const size_t lon = (static_cast<size_t>(d[pos]) << 8) | d[pos + 1];
        if (lon < 2) return false;
        const bool es_sof = marcador >= 0xC0 && marcador <= 0xCF &&
                            marcador != 0xC4 && marcador != 0xC8 &&
                            marcador != 0xCC;
        if (es_sof) {
            if (pos + 2 + 5 > n || lon < 8) return false;
            *alto = (static_cast<int>(d[pos + 3]) << 8) | d[pos + 4];
            *ancho = (static_cast<int>(d[pos + 5]) << 8) | d[pos + 6];
            return *ancho > 0 && *alto > 0;
        }
        pos += lon;
    }
    return false;
}

} // namespace fusion::imgexp
