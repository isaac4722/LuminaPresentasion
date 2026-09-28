// src/core/src/core/PptxDirecto.cpp — Lector directo de paquetes
// .pptx/.pptm (ISO/IEC-29500). Portable, sin dependencias nuevas:
//   1) CRC32 + inflador RFC 1951 propios (almacenado, fijo y dinámico).
//   2) Lector ZIP mínimo vía directorio central (con ZIP64 defensivo).
//   3) Mini-extractor XML (etiquetas, atributos, entidades).
//   4) Recorrido del paquete OPC: _rels/.rels → presentation.xml →
//      relaciones → p:sldIdLst → ppt/slides/slideN.xml.
// Leer no ejecuta nada del paquete (regla .pptm: las macros jamás).

#include "fusion/core/PptxDirecto.h"

#include "ZipInterno.h"  // Crc32 + lecturas LE compartidas (fusion::zipint)

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fusion {

namespace {

// ---------------------------------------------------------------------------
// Lecturas little-endian y CRC32: viven en ZipInterno.h (fusion::zipint),
// compartidas con el escritor de paquetes.
// ---------------------------------------------------------------------------

using fusion::zipint::Leer16;
using fusion::zipint::Leer32;
using fusion::zipint::Leer64;
using fusion::zipint::Crc32;

using fusion::zipint::Inflar;

// ---------------------------------------------------------------------------
// Lector ZIP mínimo (directorio central; soporta almacenado + deflate,
// ZIP64 defensivo y verificación CRC32).
// ---------------------------------------------------------------------------

struct EntradaZip {
    std::string nombre;
    uint16_t   metodo = 0;
    uint32_t   crc = 0;
    uint64_t   tam_comp = 0;
    uint64_t   tam_real = 0;
    uint64_t   offset_local = 0;
};

class LectorZip {
public:
    bool Abrir(const unsigned char* bytes, size_t tam, std::string* error) {
        b_ = bytes;
        tam_ = tam;
        if (tam < 22) {
            if (error) *error = "no es un paquete zip (demasiado corto)";
            return false;
        }

        // 1) Localizar el registro EOCD escaneando hacia atrás (los
        //    escritores pueden añadir comentario hasta 64 KiB).
        size_t eocd = 0;
        bool hallado = false;
        const size_t minimo = tam >= 65557 ? tam - 65557 : 0;
        for (size_t p = tam - 22;; --p) {
            if (Leer32(b_, tam_, p) == 0x06054b50u) {
                const uint16_t comentario = Leer16(b_, tam_, p + 20);
                if (p + 22 + comentario <= tam) {
                    eocd = p;
                    hallado = true;
                    break;
                }
            }
            if (p == minimo) break;
        }
        if (!hallado) {
            if (error)
                *error = "no es un paquete zip (sin registro EOCD)";
            return false;
        }

        uint64_t total = Leer16(b_, tam_, eocd + 10);
        uint64_t cd_tam = Leer32(b_, tam_, eocd + 12);
        uint64_t cd_off = Leer32(b_, tam_, eocd + 16);

        // 2) ZIP64: si algún campo del EOCD se quedó corto, tomarlo del
        //    registro ZIP64 (localizador justo antes del EOCD).
        if (total == 0xFFFFu || cd_tam == 0xFFFFFFFFu ||
            cd_off == 0xFFFFFFFFu) {
            if (eocd >= 20 &&
                Leer32(b_, tam_, eocd - 20) == 0x07064b50u) {
                const uint64_t z64 = Leer64(b_, tam_, eocd - 20 + 8);
                if (z64 + 56 <= tam &&
                    Leer32(b_, tam_, static_cast<size_t>(z64)) ==
                        0x06064b50u) {
                    total  = Leer64(b_, tam_, static_cast<size_t>(z64) + 32);
                    cd_tam = Leer64(b_, tam_, static_cast<size_t>(z64) + 40);
                    cd_off = Leer64(b_, tam_, static_cast<size_t>(z64) + 48);
                }
            }
        }

        // 3) Recorrer el directorio central.
        entradas_.clear();
        uint64_t pos = cd_off;
        for (uint64_t i = 0; i < total; ++i) {
            const size_t p = static_cast<size_t>(pos);
            if (p + 46 > tam || Leer32(b_, tam_, p) != 0x02014b50u) {
                if (error)
                    *error = "directorio central del paquete corrupto";
                return false;
            }
            EntradaZip e;
            e.metodo = Leer16(b_, tam_, p + 10);
            e.crc = Leer32(b_, tam_, p + 16);
            e.tam_comp = Leer32(b_, tam_, p + 20);
            e.tam_real = Leer32(b_, tam_, p + 24);
            const uint16_t nlen = Leer16(b_, tam_, p + 28);
            const uint16_t elen = Leer16(b_, tam_, p + 30);
            const uint16_t clen = Leer16(b_, tam_, p + 32);
            e.offset_local = Leer32(b_, tam_, p + 42);
            if (p + 46 + nlen > tam) {
                if (error)
                    *error = "directorio central del paquete corrupto";
                return false;
            }
            e.nombre.assign(reinterpret_cast<const char*>(b_ + p + 46), nlen);

            // Campo extra ZIP64 por entrada (tamaños/offset que no cupieron).
            size_t xe = p + 46 + nlen;
            const size_t xe_fin = xe + elen <= tam ? xe + elen : tam;
            while (xe + 4 <= xe_fin) {
                const uint16_t id = Leer16(b_, tam_, xe);
                const uint16_t sz = Leer16(b_, tam_, xe + 2);
                if (id == 0x0001 && sz >= 24) {
                    size_t x = xe + 4;
                    if (e.tam_real == 0xFFFFFFFFu && x + 8 <= xe_fin) {
                        e.tam_real = Leer64(b_, tam_, x);
                        x += 8;
                    }
                    if (e.tam_comp == 0xFFFFFFFFu && x + 8 <= xe_fin) {
                        e.tam_comp = Leer64(b_, tam_, x);
                        x += 8;
                    }
                    if (e.offset_local == 0xFFFFFFFFu && x + 8 <= xe_fin)
                        e.offset_local = Leer64(b_, tam_, x);
                    break;
                }
                xe += 4 + sz;
            }

            entradas_.push_back(e);
            pos += 46 + nlen + elen + clen;
        }
        return true;
    }

    // Extrae por nombre exacto. Verifica método, tamaño y CRC32.
    bool Extraer(const std::string& nombre, std::string* contenido,
                 std::string* error) const {
        const EntradaZip* e = nullptr;
        for (const auto& it : entradas_)
            if (it.nombre == nombre) {
                e = &it;
                break;
            }
        if (!e) {
            if (error) *error = "el paquete no contiene '" + nombre + "'";
            return false;
        }

        const size_t cab = static_cast<size_t>(e->offset_local);
        if (cab + 30 > tam_ || Leer32(b_, tam_, cab) != 0x04034b50u) {
            if (error)
                *error = "entrada '" + nombre + "' con cabecera local rota";
            return false;
        }
        const uint16_t nlen = Leer16(b_, tam_, cab + 26);
        const uint16_t elen = Leer16(b_, tam_, cab + 28);
        const uint64_t ini_dato = cab + 30 + nlen + elen;
        if (ini_dato > tam_ || e->tam_comp > tam_ - ini_dato) {
            if (error)
                *error = "entrada '" + nombre + "' truncada";
            return false;
        }
        const unsigned char* dato = b_ + ini_dato;
        const size_t ncomp = static_cast<size_t>(e->tam_comp);

        std::vector<unsigned char> out;
        if (e->metodo == 0) {
            if (ncomp != static_cast<size_t>(e->tam_real)) {
                if (error)
                    *error = "entrada '" + nombre +
                             "' con tamaños incoherentes (almacenada)";
                return false;
            }
            out.assign(dato, dato + ncomp);
        } else if (e->metodo == 8) {
            if (!Inflar(dato, ncomp, static_cast<size_t>(e->tam_real), out)) {
                if (error)
                    *error = "entrada '" + nombre +
                             "' con datos deflate corruptos";
                return false;
            }
        } else {
            if (error)
                *error = "método de compresión no soportado en '" + nombre +
                         "'";
            return false;
        }

        if (Crc32(out.data(), out.size()) != e->crc) {
            if (error) *error = "crc32 no coincide en '" + nombre + "'";
            return false;
        }
        contenido->assign(reinterpret_cast<const char*>(out.data()),
                          out.size());
        return true;
    }

private:
    const unsigned char* b_ = nullptr;
    size_t tam_ = 0;
    std::vector<EntradaZip> entradas_;
};

// ---------------------------------------------------------------------------
// Mini-extractor XML: aperturas con frontera de nombre, atributos y
// entidades. Suficiente para las partes de diapositivas (XML bien
// formado por especificación).
// ---------------------------------------------------------------------------

constexpr size_t kNoHallado = static_cast<size_t>(-1);

// Posición de la siguiente apertura "<nombre" cuyo carácter siguiente es
// frontera ('>' ' ' '\t' '\n' '\r' '/'). Evita confundir <a:t> con
// <a:tbl> o <p:sp> con <p:spPr>.
size_t HallarApertura(const std::string& xml, size_t desde, size_t hasta,
                      const std::string& nombre) {
    const std::string marca = "<" + nombre;
    size_t pos = desde;
    while (pos < hasta) {
        pos = xml.find(marca, pos);
        if (pos == std::string::npos || pos >= hasta) return kNoHallado;
        const char c = pos + marca.size() < xml.size()
                           ? xml[pos + marca.size()]
                           : '\0';
        if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '/')
            return pos;
        pos += marca.size();
    }
    return kNoHallado;
}

// Itera cada elemento (incluidos los self-closing) dentro de [desde,hasta):
// fn(inicio_apertura, inicio_contenido, fin_contenido).
template <typename F>
void CadaEtiqueta(const std::string& xml, size_t desde, size_t hasta,
                  const std::string& nombre, F fn) {
    size_t pos = desde;
    const std::string cierre = "</" + nombre + ">";
    for (;;) {
        const size_t ab = HallarApertura(xml, pos, hasta, nombre);
        if (ab == kNoHallado) return;
        const size_t fin_tag = xml.find('>', ab);
        if (fin_tag == std::string::npos || fin_tag >= hasta) return;
        if (xml[fin_tag - 1] == '/') {  // self-closing, contenido vacío
            fn(ab, fin_tag + 1, fin_tag + 1);
            pos = fin_tag + 1;
            continue;
        }
        const size_t ci = xml.find(cierre, fin_tag);
        if (ci == std::string::npos || ci >= hasta) return;
        fn(ab, fin_tag + 1, ci);
        pos = ci + cierre.size();
    }
}

// Texto de la etiqueta que abre en `ini` (desde '<' hasta '>' incluido).
std::string TextoEtiqueta(const std::string& xml, size_t ini) {
    const size_t fin = xml.find('>', ini);
    if (fin == std::string::npos) return "";
    return xml.substr(ini, fin - ini + 1);
}

std::string AtributoDe(const std::string& tag, const std::string& nombre) {
    const std::string patron = " " + nombre + "=";
    size_t p = tag.find(patron);
    if (p == std::string::npos) return "";
    p += patron.size();
    if (p >= tag.size()) return "";
    const char q = tag[p];
    if (q != '"' && q != '\'') return "";
    const size_t fin = tag.find(q, p + 1);
    if (fin == std::string::npos) return "";
    return tag.substr(p + 1, fin - p - 1);
}

std::string DecodificarXml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] != '&') {
            out += s[i++];
            continue;
        }
        const size_t pc = s.find(';', i);
        const size_t dist = pc == std::string::npos ? kNoHallado : pc - i;
        if (dist == kNoHallado || dist > 10) {
            out += s[i++];
            continue;
        }
        const std::string ent = s.substr(i, dist + 1);
        if (ent == "&lt;")        out += '<',  i += dist + 1;
        else if (ent == "&gt;")   out += '>',  i += dist + 1;
        else if (ent == "&amp;")  out += '&',  i += dist + 1;
        else if (ent == "&quot;") out += '"',  i += dist + 1;
        else if (ent == "&apos;") out += '\'', i += dist + 1;
        else if (ent.compare(0, 3, "&#x") == 0 || ent.compare(0, 2, "&#") == 0) {
            const char* ini_num = ent.c_str() + (ent[2] == 'x' ? 3 : 2);
            char* fin_num = nullptr;
            const long v = std::strtol(ini_num, &fin_num, ent[2] == 'x' ? 16 : 10);
            if (fin_num && *fin_num == ';' && v > 0 && v <= 0x10FFFF) {
                // UTF-8 manual (evita dependencias).
                unsigned long cp = static_cast<unsigned long>(v);
                if (cp < 0x80) {
                    out += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    out += static_cast<char>(0xC0 | (cp >> 6));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    out += static_cast<char>(0xE0 | (cp >> 12));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    out += static_cast<char>(0xF0 | (cp >> 18));
                    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
                i += dist + 1;
            } else {
                out += s[i++];
            }
        } else {
            out += s[i++];  // entidad desconocida: dejar tal cual
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Rutas de paquete OPC
// ---------------------------------------------------------------------------

std::string DirBase(const std::string& parte) {
    const size_t p = parte.find_last_of('/');
    return p == std::string::npos ? "" : parte.substr(0, p + 1);
}

// Resuelve el Target de una relación contra el directorio de la parte.
// Acepta absolutos "/ppt/..." y sube "../".
std::string ResolverRuta(const std::string& base_dir,
                         const std::string& destino) {
    std::string ruta = destino;
    if (!ruta.empty() && ruta[0] == '/') return ruta.substr(1);
    std::vector<std::string> segs;
    size_t i = 0;
    while (i < base_dir.size()) {
        const size_t f = base_dir.find('/', i);
        if (f == std::string::npos) break;
        segs.push_back(base_dir.substr(i, f - i));
        i = f + 1;
    }
    i = 0;
    while (i <= ruta.size()) {
        size_t f = ruta.find('/', i);
        if (f == std::string::npos) {
            if (i < ruta.size()) segs.push_back(ruta.substr(i));
            break;
        }
        segs.push_back(ruta.substr(i, f - i));
        i = f + 1;
    }
    std::vector<std::string> pila;
    for (const auto& s : segs) {
        if (s.empty() || s == ".") continue;
        if (s == "..") {
            if (!pila.empty()) pila.pop_back();
            continue;
        }
        pila.push_back(s);
    }
    std::string out;
    for (size_t k = 0; k < pila.size(); ++k) {
        if (k) out += '/';
        out += pila[k];
    }
    return out;
}

// Ruta de relaciones de una parte: ppt/slides/slide1.xml ->
// ppt/slides/_rels/slide1.xml.rels
std::string RutaRelaciones(const std::string& parte) {
    const size_t p = parte.find_last_of('/');
    const std::string dir = p == std::string::npos ? "" : parte.substr(0, p);
    const std::string nombre =
        p == std::string::npos ? parte : parte.substr(p + 1);
    return (dir.empty() ? "" : dir + "/") + "_rels/" + nombre + ".rels";
}

struct Relacion {
    std::string id;
    std::string tipo;        // Type completo del esquema
    std::string destino;     // resuelto contra la parte origen
    bool externa = false;
};

std::vector<Relacion> LeerRelaciones(const std::string& xml,
                                     const std::string& parte_origen) {
    std::vector<Relacion> rels;
    CadaEtiqueta(xml, 0, xml.size(), "Relationship",
                 [&](size_t ini, size_t, size_t) {
                     const std::string tag = TextoEtiqueta(xml, ini);
                     Relacion r;
                     r.id = AtributoDe(tag, "Id");
                     r.tipo = AtributoDe(tag, "Type");
                     r.externa = AtributoDe(tag, "TargetMode") == "External";
                     r.destino = ResolverRuta(DirBase(parte_origen),
                                              AtributoDe(tag, "Target"));
                     if (!r.id.empty() && !r.externa) rels.push_back(r);
                 });
    return rels;
}

// ¿El Type termina con el sufijo dado? ("/officeDocument", "/slide")
bool TipoTerminaEn(const Relacion& r, const std::string& sufijo) {
    return r.tipo.size() >= sufijo.size() &&
           r.tipo.compare(r.tipo.size() - sufijo.size(), sufijo.size(),
                          sufijo) == 0;
}

const Relacion* BuscarRelacion(const std::vector<Relacion>& rels,
                               const std::string& id) {
    for (const auto& r : rels)
        if (r.id == id) return &r;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Lectura de diapositivas
// ---------------------------------------------------------------------------

// Texto de un párrafo: concatenación de sus runs <a:t> (los runs son
// fragmentos de un mismo párrafo; se unen sin separador).
std::string TextoParrafo(const std::string& xml, size_t ini, size_t fin) {
    std::string p;
    CadaEtiqueta(xml, ini, fin, "a:t",
                 [&](size_t, size_t ic, size_t fc) {
                     p += DecodificarXml(xml.substr(ic, fc - ic));
                 });
    return p;
}

DiapositivaPptx LeerDiapositiva(const std::string& xml, int indice) {
    DiapositivaPptx d;
    d.indice = indice;
    bool titulo_tomado = false;

    // 1) Formas (p:sp): el placeholder title/ctrTitle da el título; el
    //    resto de párrafos van al cuerpo en orden de lectura.
    CadaEtiqueta(xml, 0, xml.size(), "p:sp",
                 [&](size_t, size_t ic, size_t fc) {
                     bool es_titulo = false;
                     const size_t ph = HallarApertura(xml, ic, fc, "p:ph");
                     if (ph != kNoHallado) {
                         const std::string tipo =
                             AtributoDe(TextoEtiqueta(xml, ph), "type");
                         es_titulo = (tipo == "title" || tipo == "ctrTitle");
                     }
                     std::vector<std::string> parrafos;
                     CadaEtiqueta(xml, ic, fc, "a:p",
                                  [&](size_t, size_t ip, size_t fp) {
                                      std::string t = TextoParrafo(xml, ip, fp);
                                      if (!t.empty())
                                          parrafos.push_back(std::move(t));
                                  });
                     if (es_titulo && !titulo_tomado) {
                         titulo_tomado = true;
                         for (const auto& t : parrafos) {
                             if (!d.titulo.empty()) d.titulo += " ";
                             d.titulo += t;
                         }
                     } else {
                         for (auto& t : parrafos)
                             d.parrafos.push_back(std::move(t));
                     }
                 });

    // 2) Marcos gráficos (p:graphicFrame): tablas (a:tbl) y otros.
    //    Sus celdas aportan párrafos al cuerpo (después de las formas;
    //    orden entre tipos simplificado, v1).
    CadaEtiqueta(xml, 0, xml.size(), "p:graphicFrame",
                 [&](size_t, size_t ic, size_t fc) {
                     CadaEtiqueta(xml, ic, fc, "a:p",
                                  [&](size_t, size_t ip, size_t fp) {
                                      std::string t = TextoParrafo(xml, ip, fp);
                                      if (!t.empty())
                                          d.parrafos.push_back(std::move(t));
                                  });
                 });
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

InfoPptx LectorPptx::LeerDesdeMemoria(const unsigned char* bytes, size_t tam,
                                      std::vector<DiapositivaPptx>* salida) {
    InfoPptx r;
    if (salida) salida->clear();

    LectorZip zip;
    std::string err;
    if (!zip.Abrir(bytes, tam, &err)) {
        r.msg_error = err;
        return r;
    }

    // 1) _rels/.rels: documento de presentación por Type (officeDocument).
    std::string rels_raiz;
    if (!zip.Extraer("_rels/.rels", &rels_raiz, &err)) {
        r.msg_error = err;
        return r;
    }
    std::string parte_doc;
    for (const auto& rel : LeerRelaciones(rels_raiz, "")) {
        if (TipoTerminaEn(rel, "/officeDocument")) {
            parte_doc = rel.destino;
            break;
        }
    }
    if (parte_doc.empty()) {
        r.msg_error = "el paquete no declara un documento de presentación "
                      "(officeDocument)";
        return r;
    }

    // 2) presentation.xml + sus relaciones.
    std::string pres;
    if (!zip.Extraer(parte_doc, &pres, &err)) {
        r.msg_error = err;
        return r;
    }
    const std::string ruta_rels = RutaRelaciones(parte_doc);
    std::vector<Relacion> rels_pres;
    bool rels_faltan = false;
    {
        std::string rels_pres_xml;
        if (!zip.Extraer(ruta_rels, &rels_pres_xml, &err))
            rels_faltan = true;  // tolerado si no hay diapositivas declaradas
        else
            rels_pres = LeerRelaciones(rels_pres_xml, parte_doc);
    }

    // 3) Tamaño de diapositiva en EMU (p:sldSz) para la relación de
    //    aspecto del render.
    {
        const size_t sz = HallarApertura(pres, 0, pres.size(), "p:sldSz");
        if (sz != kNoHallado) {
            const std::string tag = TextoEtiqueta(pres, sz);
            r.ancho_emu = std::strtoll(AtributoDe(tag, "cx").c_str(),
                                       nullptr, 10);
            r.alto_emu = std::strtoll(AtributoDe(tag, "cy").c_str(),
                                      nullptr, 10);
        }
    }

    // 4) Orden real de proyección: p:sldIdLst → p:sldId (r:id) →
    //    relación de tipo .../slide → parte.
    std::vector<std::string> partes_slides;
    int sld_ids_declarados = 0;
    const size_t ini_lst = HallarApertura(pres, 0, pres.size(), "p:sldIdLst");
    if (ini_lst != kNoHallado) {
        const size_t fin_tag = pres.find('>', ini_lst);
        size_t hasta = pres.size();
        if (fin_tag != std::string::npos) {
            const size_t fin_lst = pres.find("</p:sldIdLst>", fin_tag);
            if (fin_lst != std::string::npos) hasta = fin_lst;
        }
        CadaEtiqueta(pres, fin_tag + 1, hasta, "p:sldId",
                     [&](size_t ini, size_t, size_t) {
                         ++sld_ids_declarados;
                         const std::string rid =
                             AtributoDe(TextoEtiqueta(pres, ini), "r:id");
                         if (rid.empty()) return;
                         const Relacion* rel = BuscarRelacion(rels_pres, rid);
                         if (!rel || !TipoTerminaEn(*rel, "/slide")) return;
                         partes_slides.push_back(rel->destino);
                     });
    }
    // Diapositivas declaradas sin parte de relaciones = paquete roto.
    if (rels_faltan && sld_ids_declarados > 0) {
        r.msg_error = "el paquete no contiene '" + ruta_rels + "'";
        return r;
    }
    if (partes_slides.empty())
        r.avisos.push_back("el paquete no tiene diapositivas en p:sldIdLst");

    // 5) Leer cada diapositiva en su posición de la lista. Una parte
    //    ausente o corrupta se informa como aviso y se omite; el índice
    //    mantiene la posición original (fidelidad con el archivo).
    int leidas = 0;
    for (size_t i = 0; i < partes_slides.size(); ++i) {
        std::string xml_slide;
        if (!zip.Extraer(partes_slides[i], &xml_slide, &err)) {
            r.avisos.push_back("diapositiva " + std::to_string(i + 1) +
                               " omitida: " + err);
            continue;
        }
        DiapositivaPptx d = LeerDiapositiva(xml_slide,
                                            static_cast<int>(i) + 1);
        if (salida) salida->push_back(std::move(d));
        ++leidas;
    }
    r.total_diapositivas = leidas;
    r.ok = true;
    return r;
}

InfoPptx LectorPptx::LeerArchivo(const std::string& ruta,
                                 std::vector<DiapositivaPptx>* salida) {
    InfoPptx r;
    std::FILE* f = std::fopen(ruta.c_str(), "rb");
    if (!f) {
        r.msg_error = "no se pudo abrir '" + ruta + "'";
        return r;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        r.msg_error = "no se pudo medir '" + ruta + "'";
        return r;
    }
    const long tam = std::ftell(f);
    if (tam < 0 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        r.msg_error = "no se pudo medir '" + ruta + "'";
        return r;
    }
    std::vector<unsigned char> datos(static_cast<size_t>(tam));
    const size_t leidos =
        datos.empty() ? 0 : std::fread(datos.data(), 1, datos.size(), f);
    std::fclose(f);
    if (leidos != datos.size()) {
        r.msg_error = "lectura incompleta de '" + ruta + "'";
        return r;
    }
    return LeerDesdeMemoria(datos.data(), datos.size(), salida);
}

bool LectorPptx::EsPaquetePptx(const std::string& ruta) {
    const auto pos = ruta.find_last_of('.');
    if (pos == std::string::npos || pos == 0 || pos + 1 == ruta.size())
        return false;
    std::string ext = ruta.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext == ".pptx" || ext == ".pptm";
}

} // namespace fusion
