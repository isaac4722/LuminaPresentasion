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
// Adler32 (RFC 1950) para flujos zlib.
// ---------------------------------------------------------------------------

inline uint32_t Adler32(const unsigned char* d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

inline void Poner32BE(std::string* s, uint32_t v) {
    s->push_back(static_cast<char>((v >> 24) & 0xFF));
    s->push_back(static_cast<char>((v >> 16) & 0xFF));
    s->push_back(static_cast<char>((v >> 8) & 0xFF));
    s->push_back(static_cast<char>(v & 0xFF));
}

// Flujo zlib (RFC 1950): cabecera 0x78 0x01 + bloques almacenados +
// adler32 en orden de red. Lo usan el IDAT del CodificarPng y el
// /FlateDecode de los XObject del PDF (imágenes RGB crudas).
inline std::string ZlibAlmacenado(const unsigned char* d, size_t n) {
    std::string out;
    out.push_back(static_cast<char>(0x78));
    out.push_back(static_cast<char>(0x01));
    size_t i = 0;
    while (i < n) {
        const size_t bloque = (n - i > 65535) ? 65535 : (n - i);
        const bool ultimo = (i + bloque == n);
        out.push_back(ultimo ? static_cast<char>(1) : static_cast<char>(0));
        out.push_back(static_cast<char>(bloque & 0xFF));
        out.push_back(static_cast<char>((bloque >> 8) & 0xFF));
        out.push_back(static_cast<char>(~bloque & 0xFF));
        out.push_back(static_cast<char>((~bloque >> 8) & 0xFF));
        out.append(reinterpret_cast<const char*>(d + i), bloque);
        i += bloque;
    }
    if (n == 0) {  // un bloque almacenado vacío también es válido
        out.push_back(static_cast<char>(1));
        out.push_back(0); out.push_back(0);
        out.push_back(static_cast<char>(0xFF));
        out.push_back(static_cast<char>(0xFF));
    }
    Poner32BE(&out, Adler32(d, n));
    return out;
}

// ---------------------------------------------------------------------------
// Inflador RFC 1951 (deflate crudo). Estilo canónico: cuenta por longitud
// + símbolos ordenados; decodificación bit a bit desde el LSB.
// ---------------------------------------------------------------------------

namespace {

struct Bits {
    const unsigned char* d = nullptr;
    size_t n = 0;
    size_t pos = 0;
    uint32_t acc = 0;
    int cnt = 0;
    bool err = false;

    int LeerBit() {
        if (cnt == 0) {
            if (pos >= n) {
                err = true;
                return 0;
            }
            acc = d[pos++];
            cnt = 8;
        }
        const int bit = static_cast<int>(acc & 1u);
        acc >>= 1;
        --cnt;
        return bit;
    }

    int LeerBits(int k) {
        int v = 0;
        for (int i = 0; i < k; ++i) v |= LeerBit() << i;
        return v;
    }
};

struct Huff {
    uint16_t cuenta[16] = {0};    // códigos por longitud
    uint16_t simbolo[288] = {0};  // símbolos ordenados por (longitud, código)
};

bool ConstruirHuff(const uint8_t* longitudes, int n, Huff& h) {
    for (int i = 0; i < 16; ++i) h.cuenta[i] = 0;
    for (int i = 0; i < n; ++i) h.cuenta[longitudes[i]]++;
    if (h.cuenta[0] == n) return true;  // árbol vacío (válido en dist)
    int izquierda = 1;
    for (int len = 1; len <= 15; ++len) {
        izquierda <<= 1;
        izquierda -= h.cuenta[len];
        if (izquierda < 0) return false;  // código sobreasignado
    }
    uint16_t offsets[16] = {0};
    for (int len = 1; len < 15; ++len)
        offsets[len + 1] = static_cast<uint16_t>(offsets[len] + h.cuenta[len]);
    for (int sim = 0; sim < n; ++sim)
        if (longitudes[sim] != 0)
            h.simbolo[offsets[longitudes[sim]]++] =
                static_cast<uint16_t>(sim);
    return true;
}

int Decodificar(const Huff& h, Bits& b) {
    int codigo = 0, primero = 0, indice = 0;
    for (int len = 1; len <= 15; ++len) {
        codigo |= b.LeerBit();
        if (b.err) return -1;
        const int c = h.cuenta[len];
        if (codigo - primero < c)
            return h.simbolo[indice + (codigo - primero)];
        indice += c;
        primero = (primero + c) << 1;
        codigo <<= 1;
    }
    b.err = true;
    return -1;
}

const uint16_t kBaseLen[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19,
                               23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115,
                               131, 163, 195, 227, 258};
const uint8_t kExtraLen[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kBaseDist[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65,
                                97, 129, 193, 257, 385, 513, 769, 1025, 1537,
                                2049, 3073, 4097, 6145, 8193, 12289, 16385,
                                24577};
const uint8_t kExtraDist[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12,
                                13, 13};

} // namespace

inline bool Inflar(const unsigned char* in, size_t nIn, size_t tamEsperado,
                   std::vector<unsigned char>& out) {
    out.clear();
    if (tamEsperado > (256u << 20)) return false;  // antizip-bomb: 256 MiB
    if (tamEsperado == 0) return in != nullptr;
    out.reserve(tamEsperado);
    Bits b;
    b.d = in;
    b.n = nIn;

    bool ultimo = false;
    while (!ultimo) {
        ultimo = b.LeerBit() != 0;
        const int tipo = b.LeerBits(2);
        if (b.err) return false;

        if (tipo == 0) {
            // Bloque almacenado: alinear a byte y copiar.
            b.acc = 0;
            b.cnt = 0;
            if (b.pos + 4 > nIn) return false;
            const unsigned lon =
                in[b.pos] | (static_cast<unsigned>(in[b.pos + 1]) << 8);
            const unsigned nlon =
                in[b.pos + 2] | (static_cast<unsigned>(in[b.pos + 3]) << 8);
            b.pos += 4;
            if ((lon ^ 0xFFFFu) != nlon) return false;
            if (b.pos + lon > nIn) return false;
            if (out.size() + lon > tamEsperado) return false;
            out.insert(out.end(), in + b.pos, in + b.pos + lon);
            b.pos += lon;
        } else if (tipo == 1 || tipo == 2) {
            Huff lit, dist;
            if (tipo == 1) {
                uint8_t lon[288];
                int i = 0;
                for (; i < 144; ++i) lon[i] = 8;
                for (; i < 256; ++i) lon[i] = 9;
                for (; i < 280; ++i) lon[i] = 7;
                for (; i < 288; ++i) lon[i] = 8;
                uint8_t lond[30];
                for (i = 0; i < 30; ++i) lond[i] = 5;
                if (!ConstruirHuff(lon, 288, lit)) return false;
                if (!ConstruirHuff(lond, 30, dist)) return false;
            } else {
                const int hlit = b.LeerBits(5) + 257;
                const int hdist = b.LeerBits(5) + 1;
                const int hclen = b.LeerBits(4) + 4;
                if (b.err || hlit > 286 || hdist > 32) return false;
                static const uint8_t orden[19] = {16, 17, 18, 0, 8, 7, 9, 6,
                                                  10, 5, 11, 4, 12, 3, 13, 2,
                                                  14, 1, 15};
                uint8_t loncl[19] = {0};
                for (int i = 0; i < hclen; ++i)
                    loncl[orden[i]] = static_cast<uint8_t>(b.LeerBits(3));
                Huff cl;
                if (!ConstruirHuff(loncl, 19, cl)) return false;
                uint8_t lon[286 + 32];
                int i = 0;
                while (i < hlit + hdist) {
                    const int s = Decodificar(cl, b);
                    if (s < 0) return false;
                    if (s < 16) {
                        lon[i++] = static_cast<uint8_t>(s);
                        continue;
                    }
                    int rep = 0;
                    uint8_t val = 0;
                    if (s == 16) {
                        if (i == 0) return false;
                        val = lon[i - 1];
                        rep = 3 + b.LeerBits(2);
                    } else if (s == 17) {
                        val = 0;
                        rep = 3 + b.LeerBits(3);
                    } else {
                        val = 0;
                        rep = 11 + b.LeerBits(7);
                    }
                    if (i + rep > hlit + hdist) return false;
                    while (rep-- > 0) lon[i++] = val;
                }
                if (b.err) return false;
                if (lon[256] == 0) return false;  // sin fin de bloque
                if (!ConstruirHuff(lon, hlit, lit)) return false;
                if (!ConstruirHuff(lon + hlit, hdist, dist)) return false;
            }

            for (;;) {
                const int s = Decodificar(lit, b);
                if (s < 0) return false;
                if (s < 256) {
                    if (out.size() >= tamEsperado) return false;
                    out.push_back(static_cast<unsigned char>(s));
                } else if (s == 256) {
                    break;
                } else {
                    const int li = s - 257;
                    if (li >= 29) return false;
                    const int lon2 = kBaseLen[li] + b.LeerBits(kExtraLen[li]);
                    const int ds = Decodificar(dist, b);
                    if (ds < 0 || ds >= 30) return false;
                    const int d = kBaseDist[ds] + b.LeerBits(kExtraDist[ds]);
                    if (b.err) return false;
                    if (static_cast<size_t>(d) > out.size()) return false;
                    if (out.size() + static_cast<size_t>(lon2) > tamEsperado)
                        return false;
                    const size_t src = out.size() - static_cast<size_t>(d);
                    for (int i = 0; i < lon2; ++i)
                        out.push_back(out[src + i]);  // solapado byte a byte
                }
            }
        } else {
            return false;  // tipo 3: reservado/inválido
        }
        if (b.err) return false;
    }
    return !b.err && out.size() == tamEsperado;
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
