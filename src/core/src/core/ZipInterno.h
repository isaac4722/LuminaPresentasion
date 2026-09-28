// src/core/src/core/ZipInterno.h — Piezas ZIP compartidas del núcleo.
//
// Interno (no es API pública). Lo usan:
//   - PptxDirecto.cpp   (lector de paquetes .pptx/.pptm)
//   - ExportadorPptx.cpp (escritor de paquetes .pptx)
//
// El escritor produce paquetes con entradas ALMACENADAS (método 0):
// un .pptx válido según la especificación OPC; el ahorro de espacio no
// es el objetivo de una exportación y evita implementar un deflactor.
// El lector ya soporta almacenadas y deflate.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fusion::zipint {

// ---------------------------------------------------------------------------
// Lecturas little-endian acotadas
// ---------------------------------------------------------------------------

inline uint16_t Leer16(const unsigned char* b, size_t tam, size_t pos) {
    if (pos + 2 > tam) return 0;
    return static_cast<uint16_t>(b[pos] | (b[pos + 1] << 8));
}

inline uint32_t Leer32(const unsigned char* b, size_t tam, size_t pos) {
    if (pos + 4 > tam) return 0;
    return static_cast<uint32_t>(b[pos]) |
           (static_cast<uint32_t>(b[pos + 1]) << 8) |
           (static_cast<uint32_t>(b[pos + 2]) << 16) |
           (static_cast<uint32_t>(b[pos + 3]) << 24);
}

inline uint64_t Leer64(const unsigned char* b, size_t tam, size_t pos) {
    if (pos + 8 > tam) return 0;
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | static_cast<uint64_t>(b[pos + i]);
    return v;
}

// ---------------------------------------------------------------------------
// CRC32 (polinomio 0xEDB88320, reflejado). Es el mismo CRC del ZIP y el
// de los chunks PNG, por eso vive aquí.
// ---------------------------------------------------------------------------

inline uint32_t Crc32(const unsigned char* d, size_t n) {
    static uint32_t tabla[256];
    static bool lista = false;
    if (!lista) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tabla[i] = c;
        }
        lista = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i)
        c = tabla[(c ^ d[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

inline uint32_t Crc32(const std::string& s) {
    return Crc32(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

// ---------------------------------------------------------------------------
// EscritorZip: paquete ZIP con entradas almacenadas (método 0).
// Suficiente para OPC (.pptx). No soporta entradas > 4 GiB (devuelve
// paquete vacío en Terminar si ocurriera).
// ---------------------------------------------------------------------------

class EscritorZip {
public:
    // Agrega una entrada (nombre sin '\' y con '/'). Los datos se copian.
    void Agregar(const std::string& nombre, const std::string& datos) {
        Entrada e;
        e.nombre = nombre;
        e.crc = Crc32(datos);
        e.tam = static_cast<uint32_t>(datos.size());
        e.offset = buf_.size();
        entradas_.push_back(e);

        Poner32(0x04034b50u);              // firma cabecera local
        Poner16(20);                       // versión necesaria
        Poner16(0x0800);                   // banderas: nombres UTF-8
        Poner16(0);                        // método: almacenado
        Poner16(0x0000);                   // hora DOS (1980-01-01)
        Poner16(0x0021);                   // fecha DOS
        Poner32(e.crc);
        Poner32(e.tam);                    // tamaño comprimido
        Poner32(e.tam);                    // tamaño real
        Poner16(static_cast<uint16_t>(nombre.size()));
        Poner16(0);                        // extra local
        buf_.append(nombre);
        buf_.append(datos);
    }

    // Construye directorio central + EOCD y devuelve el paquete completo.
    // Devuelve "" si algo no cupo en 32 bits (no debería pasar en pptx).
    std::string Terminar() {
        if (entradas_.empty()) return "";
        const uint32_t cd_off = static_cast<uint32_t>(buf_.size());
        for (const auto& e : entradas_) {
            if (e.offset > 0xFFFFFFFFu) return "";
            Poner32(0x02014b50u);          // firma directorio central
            Poner16(20);                   // versión hecha por
            Poner16(20);                   // versión necesaria
            Poner16(0x0800);               // banderas
            Poner16(0);                    // método
            Poner16(0x0000);               // hora DOS
            Poner16(0x0021);               // fecha DOS
            Poner32(e.crc);
            Poner32(e.tam);
            Poner32(e.tam);
            Poner16(static_cast<uint16_t>(e.nombre.size()));
            Poner16(0);                    // extra central
            Poner16(0);                    // comentario
            Poner16(0);                    // disco de inicio
            Poner16(0);                    // atributos internos
            Poner32(0);                    // atributos externos
            Poner32(static_cast<uint32_t>(e.offset));
            buf_.append(e.nombre);
        }
        const uint32_t cd_tam =
            static_cast<uint32_t>(buf_.size() - cd_off);
        if (buf_.size() + 22 > 0xFFFFFFFFu) return "";

        Poner32(0x06054b50u);              // firma EOCD
        Poner16(0);                        // número de disco
        Poner16(0);                        // disco del directorio
        Poner16(static_cast<uint16_t>(entradas_.size()));
        Poner16(static_cast<uint16_t>(entradas_.size()));
        Poner32(cd_tam);
        Poner32(cd_off);
        Poner16(0);                        // longitud de comentario
        return buf_;
    }

private:
    struct Entrada {
        std::string nombre;
        uint32_t crc = 0;
        uint32_t tam = 0;
        size_t offset = 0;
    };

    void Poner16(uint16_t v) {
        buf_.push_back(static_cast<char>(v & 0xFF));
        buf_.push_back(static_cast<char>((v >> 8) & 0xFF));
    }
    void Poner32(uint32_t v) {
        buf_.push_back(static_cast<char>(v & 0xFF));
        buf_.push_back(static_cast<char>((v >> 8) & 0xFF));
        buf_.push_back(static_cast<char>((v >> 16) & 0xFF));
        buf_.push_back(static_cast<char>((v >> 24) & 0xFF));
    }

    std::string buf_;
    std::vector<Entrada> entradas_;
};

} // namespace fusion::zipint
