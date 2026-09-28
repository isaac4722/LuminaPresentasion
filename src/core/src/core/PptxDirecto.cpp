// src/core/src/core/PptxDirecto.cpp — Lector directo de paquetes
// .pptx/.pptm (ISO/IEC-29500). Portable:
//   1) Lector ZIP propio vía directorio central (CRC32, ZIP64 defensivo,
//      límite antizip-bomb) + inflador RFC 1951 propio.
//   2) Parseo XML con pugixml (vendido en third_party, MIT) — diapositivas
//      con runs estilizados (a:rPr), imágenes p:pic y partes OPC.
//   3) Imágenes media decodificadas a RGBA con stb_image (vendido,
//      dominio público; PNG/JPEG/BMP/GIF).
//   4) Recorrido OPC: _rels/.rels → presentation.xml → relaciones →
//      p:sldIdLst → ppt/slides/slideN.xml (+ sus _rels para las media).
// Leer no ejecuta nada del paquete (regla .pptm: las macros jamás).

#include "fusion/core/PptxDirecto.h"

#include "ZipInterno.h"  // Crc32 + lecturas LE compartidas (fusion::zipint)

#include <pugixml.hpp>

#include "StbImagen.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fusion {

namespace {

using fusion::zipint::Leer16;
using fusion::zipint::Leer32;
using fusion::zipint::Leer64;
using fusion::zipint::Crc32;
using fusion::zipint::Inflar;

// Límite de seguridad por imagen decodificada (64 MiP x 4 bytes = 256 MiB
// máx. de RGBA): los paquetes legítimos no superan esto con mucho.
constexpr long long kMaxPxImagen = 64LL * 1024 * 1024;

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
    bool Abrir(const unsigned char* b, size_t tam, std::string* error) {
        if (!b || tam < 22) {
            if (error) *error = "paquete demasiado pequeño para ser un zip";
            return false;
        }
        b_ = b;
        tam_ = tam;

        // EOCD al final (con posible comentario). ZIP64: localizador EOCD64
        // justo antes del EOCD; se acepta pero los límites siguen siendo
        // defensivos (el paquete entero ya vive en memoria).
        size_t fin_cd = 0, tam_cd = 0, n_entradas = 0;
        {
            bool hallado = false;
            if (tam >= 22) {
                for (size_t back = 0; back <= 65535; ++back) {
                    if (back + 22 > tam) break;
                    const size_t p = tam - 22 - back;
                    if (Leer32(b_, tam_, p) == 0x06054b50u) {
                        fin_cd = Leer32(b_, tam_, p + 16);
                        tam_cd = Leer32(b_, tam_, p + 12);
                        n_entradas = Leer16(b_, tam_, p + 10);
                        hallado = true;
                        break;
                    }
                }
            }
            if (!hallado) {
                if (error) *error = "sin EOCD: no es un paquete zip válido";
                return false;
            }
        }

        // Directorio central.
        if (fin_cd == 0 || fin_cd >= tam_ || fin_cd + tam_cd > tam_) {
            if (error) *error = "directorio central fuera del paquete";
            return false;
        }
        size_t pos = fin_cd;
        const size_t fin = fin_cd + tam_cd;
        entradas_.reserve(static_cast<size_t>(n_entradas) + 1);
        for (size_t i = 0; i < n_entradas && pos + 46 <= fin; ++i) {
            if (Leer32(b_, tam_, pos) != 0x02014b50u) break;
            EntradaZip e;
            e.metodo = Leer16(b_, tam_, pos + 10);
            e.crc = Leer32(b_, tam_, pos + 16);
            e.tam_comp = Leer32(b_, tam_, pos + 20);
            e.tam_real = Leer32(b_, tam_, pos + 24);
            const uint16_t nlen = Leer16(b_, tam_, pos + 28);
            const uint16_t elen = Leer16(b_, tam_, pos + 30);
            const uint16_t clen = Leer16(b_, tam_, pos + 32);
            e.offset_local = Leer32(b_, tam_, pos + 42);
            if (pos + 46 + nlen > tam) {
                if (error)
                    *error = "directorio central del paquete corrupto";
                return false;
            }
            e.nombre.assign(reinterpret_cast<const char*>(b_ + pos + 46), nlen);

            // Campo extra ZIP64 por entrada (tamaños/offset que no cupieron).
            size_t xe = pos + 46 + nlen;
            const size_t xe_fin = xe + elen <= fin ? xe + elen : fin;
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
// pugixml: carga tolerante y utilidades de nombres
// ---------------------------------------------------------------------------

// Carga un XML bien formado. Un XML roto NO aborta la lectura del
// paquete: devuelve false y el caller decide (parte dañada → aviso).
bool CargarXml(const std::string& xml, pugi::xml_document* doc) {
    if (!doc) return false;
    const pugi::xml_parse_result res =
        doc->load_buffer(xml.data(), xml.size(),
                         pugi::parse_default | pugi::parse_fragment);
    return res.status == pugi::status_ok;
}

// Nombre local de un nodo/atributo: p:sp → "sp", sz → "sz". Los
// productores legales de OPC pueden usar prefijos distintos de p:/a:/r:
// para los mismos espacios de nombres; comparar por local es lo justo.
const char* NomLocal(const pugi::xml_node& n) {
    const char* name = n.name();
    const char* c = std::strrchr(name, ':');
    return c ? c + 1 : name;
}
const char* NomLocalAttr(const pugi::xml_attribute& a) {
    const char* name = a.name();
    const char* c = std::strrchr(name, ':');
    return c ? c + 1 : name;
}

pugi::xml_attribute Attr(const pugi::xml_node& n, const char* local) {
    for (pugi::xml_attribute a = n.first_attribute(); a;
         a = a.next_attribute())
        if (std::strcmp(NomLocalAttr(a), local) == 0) return a;
    return pugi::xml_attribute();
}

// Atributo de RELACIÓN (r:id, r:embed): p:sldId lleva TAMBIÉN un
// atributo plano id="número" (el id interno del slide). Hay que
// preferir el namespaced; si no hay, se acepta el plano (producido
// así por algunos generadores, aunque no sea canónico).
pugi::xml_attribute AttrRel(const pugi::xml_node& n, const char* local) {
    for (pugi::xml_attribute a = n.first_attribute(); a;
         a = a.next_attribute()) {
        const char* name = a.name();
        const char* dos = std::strrchr(name, ':');
        if (dos && std::strcmp(dos + 1, local) == 0) return a;
    }
    return Attr(n, local);
}

// Hijo por nombre local (ignora prefijo de espacio de nombres).
pugi::xml_node Hijo(const pugi::xml_node& n, const char* local) {
    for (pugi::xml_node c = n.first_child(); c; c = c.next_sibling())
        if (c.type() == pugi::node_element &&
            std::strcmp(NomLocal(c), local) == 0)
            return c;
    return pugi::xml_node();
}

// Primer descendiente con nombre local dado (búsqueda en profundidad,
// acotada a los subárboles pequeños del OPC; no hay riesgo de explosión).
pugi::xml_node HijoProfundo(const pugi::xml_node& n, const char* local) {
    for (pugi::xml_node c = n.first_child(); c; c = c.next_sibling()) {
        if (c.type() != pugi::node_element) continue;
        if (std::strcmp(NomLocal(c), local) == 0) return c;
        pugi::xml_node d = HijoProfundo(c, local);
        if (d) return d;
    }
    return pugi::xml_node();
}

bool EsNodo(const pugi::xml_node& n, const char* local) {
    return n && std::strcmp(NomLocal(n), local) == 0;
}

long long EnteroDe(const char* s) {
    return s ? std::strtoll(s, nullptr, 10) : 0;
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
    pugi::xml_document doc;
    if (!CargarXml(xml, &doc)) return rels;
    const pugi::xml_node raiz = doc.first_child();
    for (pugi::xml_node n = raiz.first_child(); n;
         n = n.next_sibling()) {
        if (!EsNodo(n, "Relationship")) continue;
        Relacion r;
        if (const pugi::xml_attribute a = Attr(n, "Id")) r.id = a.value();
        if (const pugi::xml_attribute a = Attr(n, "Type")) r.tipo = a.value();
        r.externa = Attr(n, "TargetMode").value() ==
                    std::string("External");
        r.destino = ResolverRuta(DirBase(parte_origen),
                                 Attr(n, "Target").value());
        if (!r.id.empty() && !r.externa) rels.push_back(std::move(r));
    }
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
// Lectura de diapositivas (runs, imágenes)
// ---------------------------------------------------------------------------

// Propiedades de run (a:rPr / a:defRPr): sz (centésimas de punto),
// b/i/u (1/0), color a:solidFill → a:srgbClr @val (RRGGBB).
void LeerPr(const pugi::xml_node& pr, EstructuraRun* run) {
    if (!pr) return;
    if (const pugi::xml_attribute a = Attr(pr, "sz")) {
        run->tam_pt = static_cast<float>(std::strtod(a.value(), nullptr)) /
                      100.0f;
        run->tiene_tamano = run->tam_pt > 0.0f;
    }
    if (const pugi::xml_attribute a = Attr(pr, "b")) run->negrita = a.value() == std::string("1");
    if (const pugi::xml_attribute a = Attr(pr, "i")) run->cursiva = a.value() == std::string("1");
    if (const pugi::xml_attribute a = Attr(pr, "u")) run->subrayado = a.value() != std::string("none");
    const pugi::xml_node fill = Hijo(pr, "solidFill");
    if (fill) {
        const pugi::xml_node srgb = Hijo(fill, "srgbClr");
        if (srgb) {
            const char* v = Attr(srgb, "val").value();
            if (v && *v) {
                run->color_hex = std::string("#") + v;
                run->tiene_color = true;
            }
        }
    }
}

// Un párrafo (a:p): runs a:r (a:rPr + a:t), campos a:fld (con texto) y
// saltos a:br (espacio para no pegar palabras).
ParrafoPptx LeerParrafo(const pugi::xml_node& p) {
    ParrafoPptx out;
    for (pugi::xml_node n = p.first_child(); n; n = n.next_sibling()) {
        if (n.type() != pugi::node_element) continue;
        const char* local = NomLocal(n);
        if (std::strcmp(local, "r") == 0) {
            EstructuraRun run;
            LeerPr(Hijo(n, "rPr"), &run);
            const pugi::xml_node t = Hijo(n, "t");
            if (t) run.texto_utf8 = t.text().as_string("");
            if (!run.texto_utf8.empty())
                out.runs.push_back(std::move(run));
        } else if (std::strcmp(local, "fld") == 0) {
            // Campo (nº de diapositiva, fecha...): su texto cacheado se
            // muestra tal cual (fidelidad con lo que vería PowerPoint).
            EstructuraRun run;
            LeerPr(Hijo(n, "rPr"), &run);
            const pugi::xml_node t = Hijo(n, "t");
            if (t) run.texto_utf8 = t.text().as_string("");
            if (!run.texto_utf8.empty())
                out.runs.push_back(std::move(run));
        } else if (std::strcmp(local, "br") == 0) {
            EstructuraRun br;
            br.texto_utf8 = " ";
            out.runs.push_back(std::move(br));
        }
    }
    return out;
}

// Posición/tamaño EMU (a:xfrm con a:off y a:ext) de una forma.
void LeerXfrm(const pugi::xml_node& xfrm, long long* x, long long* y,
              long long* w, long long* h) {
    *x = *y = *w = *h = 0;
    if (!xfrm) return;
    const pugi::xml_node off = Hijo(xfrm, "off");
    const pugi::xml_node ext = Hijo(xfrm, "ext");
    if (off) {
        *x = EnteroDe(Attr(off, "x").value());
        *y = EnteroDe(Attr(off, "y").value());
    }
    if (ext) {
        *w = EnteroDe(Attr(ext, "cx").value());
        *h = EnteroDe(Attr(ext, "cy").value());
    }
}

// Decodifica una parte media a RGBA con stb_image. Error → false y
// motivo (el caller lo convierte en aviso; nada silencioso).
bool DecodificarMedia(const std::string& bytes, ImagenPptx* img,
                      std::string* motivo) {
    if (bytes.empty()) {
        *motivo = "parte media vacía";
        return false;
    }
    int w = 0, h = 0, comp = 0;
    stbi_uc* datos = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()), &w, &h, &comp, 4);
    if (!datos) {
        *motivo = std::string("formato no soportado o corrupto (") +
                  (stbi_failure_reason() ? stbi_failure_reason() : "?") + ")";
        return false;
    }
    const long long px = static_cast<long long>(w) * h;
    if (px <= 0 || px > kMaxPxImagen) {
        stbi_image_free(datos);
        *motivo = "imagen fuera del límite de tamaño";
        return false;
    }
    img->ancho = w;
    img->alto = h;
    img->rgba.assign(datos, datos + px * 4);
    stbi_image_free(datos);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

std::string ParrafoPptx::Texto() const {
    std::string t;
    for (const auto& r : runs) t += r.texto_utf8;
    return t;
}

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
        pugi::xml_document doc;
        if (CargarXml(pres, &doc)) {
            for (pugi::xml_node n = doc.first_child().first_child(); n;
                 n = n.next_sibling()) {
                if (EsNodo(n, "sldSz")) {
                    r.ancho_emu = EnteroDe(Attr(n, "cx").value());
                    r.alto_emu = EnteroDe(Attr(n, "cy").value());
                    break;
                }
            }
        } else {
            r.avisos.push_back("presentation.xml no es XML válido: sin "
                               "tamaño de diapositiva");
        }
    }

    // 4) Orden real de proyección: p:sldIdLst → p:sldId (r:id) →
    //    relación de tipo .../slide → parte.
    std::vector<std::string> partes_slides;
    int sld_ids_declarados = 0;
    {
        pugi::xml_document doc;
        if (CargarXml(pres, &doc)) {
            const pugi::xml_node raiz = doc.first_child();
            const pugi::xml_node lst = Hijo(raiz, "sldIdLst");
            if (lst) {
                for (pugi::xml_node n = lst.first_child(); n;
                     n = n.next_sibling()) {
                    if (!EsNodo(n, "sldId")) continue;
                    ++sld_ids_declarados;
                    const char* rid = AttrRel(n, "id").value();
                    if (!rid || !*rid) continue;
                    const Relacion* rel = BuscarRelacion(rels_pres, rid);
                    if (!rel || !TipoTerminaEn(*rel, "/slide")) continue;
                    partes_slides.push_back(rel->destino);
                }
            }
        } else {
            r.msg_error = "presentation.xml no es XML válido";
            return r;
        }
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
        const std::string prefijo_aviso =
            "diapositiva " + std::to_string(i + 1) + ": ";

        pugi::xml_document doc;
        if (!CargarXml(xml_slide, &doc)) {
            r.avisos.push_back(prefijo_aviso +
                               "el XML de la diapositiva está roto");
            continue;
        }

        DiapositivaPptx d;
        d.indice = static_cast<int>(i) + 1;

        // Relaciones de la diapositiva (para las media de las p:pic).
        std::vector<Relacion> rels_slide;
        {
            std::string rels_slide_xml;
            if (zip.Extraer(RutaRelaciones(partes_slides[i]),
                            &rels_slide_xml, nullptr))
                rels_slide = LeerRelaciones(rels_slide_xml,
                                            partes_slides[i]);
        }

        bool titulo_tomado = false;

        // Recorrido en orden de documento del spTree: p:sp (texto),
        // p:pic (imágenes), p:graphicFrame (tablas).
        const pugi::xml_node raiz = doc.first_child();        // p:sld
        const pugi::xml_node csld = Hijo(raiz, "cSld");
        const pugi::xml_node tree = Hijo(csld, "spTree");
        for (pugi::xml_node forma = tree.first_child(); forma;
             forma = forma.next_sibling()) {
            if (forma.type() != pugi::node_element) continue;
            const char* local = NomLocal(forma);

            if (std::strcmp(local, "sp") == 0) {
                bool es_titulo = false;
                const pugi::xml_node nv = Hijo(forma, "nvSpPr");
                // El p:ph vive en p:nvSpPr/p:nvPr/p:ph (algunos
                // productores lo anidan distinto: búsqueda en profundidad
                // dentro de nvSpPr, subárbol pequeño).
                const pugi::xml_node ph_real =
                    nv ? HijoProfundo(nv, "ph") : pugi::xml_node();
                if (ph_real) {
                    const char* tipo = Attr(ph_real, "type").value();
                    es_titulo = std::strcmp(tipo, "title") == 0 ||
                                std::strcmp(tipo, "ctrTitle") == 0;
                }

                const pugi::xml_node txbody = Hijo(forma, "txBody");
                std::vector<ParrafoPptx> parrafos;
                if (txbody) {
                    for (pugi::xml_node p = txbody.first_child(); p;
                         p = p.next_sibling()) {
                        if (!EsNodo(p, "p")) continue;
                        ParrafoPptx par = LeerParrafo(p);
                        if (!par.runs.empty())
                            parrafos.push_back(std::move(par));
                    }
                }
                if (es_titulo && !titulo_tomado) {
                    titulo_tomado = true;
                    for (const auto& par : parrafos) {
                        const std::string t = par.Texto();
                        if (t.empty()) continue;
                        if (!d.titulo.empty()) d.titulo += " ";
                        d.titulo += t;
                    }
                } else {
                    for (auto& par : parrafos)
                        d.parrafos_ricos.push_back(std::move(par));
                }
            } else if (std::strcmp(local, "pic") == 0) {
                ImagenPptx img;
                const pugi::xml_node spPr = Hijo(forma, "spPr");
                LeerXfrm(Hijo(spPr, "xfrm"), &img.x_emu, &img.y_emu,
                         &img.w_emu, &img.h_emu);

                // r:embed → relación → parte media → RGBA.
                const pugi::xml_node blipFill = Hijo(forma, "blipFill");
                const pugi::xml_node blip = Hijo(blipFill, "blip");
                const char* rid = blip ? AttrRel(blip, "embed").value() : "";
                bool lista = false;
                if (rid && *rid) {
                    const Relacion* rel = BuscarRelacion(rels_slide, rid);
                    if (rel) {
                        std::string media;
                        if (zip.Extraer(rel->destino, &media, &err)) {
                            img.parte = rel->destino;
                            std::string motivo;
                            if (DecodificarMedia(media, &img, &motivo)) {
                                d.imagenes.push_back(std::move(img));
                                lista = true;
                            } else {
                                r.avisos.push_back(
                                    prefijo_aviso + "imagen '" +
                                    rel->destino + "' no usable: " + motivo);
                            }
                        } else {
                            r.avisos.push_back(
                                prefijo_aviso + "media '" + rel->destino +
                                "' ausente del paquete");
                        }
                    } else {
                        r.avisos.push_back(prefijo_aviso +
                                           "la imagen referencia la "
                                           "relación '" + rid +
                                           "' que no existe");
                    }
                } else {
                    r.avisos.push_back(prefijo_aviso +
                                       "p:pic sin r:embed: omitida");
                }
                (void)lista;
            } else if (std::strcmp(local, "graphicFrame") == 0) {
                // La tabla vive en p:graphic/a:graphicData/a:tbl (algunos
                // productores la anidan distinto: búsqueda en profundidad
                // acotada al marco).
                const pugi::xml_node tbl = HijoProfundo(forma, "tbl");
                const pugi::xml_node origen = tbl ? tbl : forma;
                for (pugi::xml_node fila = origen.first_child(); fila;
                     fila = fila.next_sibling()) {
                    if (!EsNodo(fila, "tr")) continue;
                    for (pugi::xml_node celda = fila.first_child(); celda;
                         celda = celda.next_sibling()) {
                        if (!EsNodo(celda, "tc")) continue;
                        const pugi::xml_node txbody = Hijo(celda, "txBody");
                        if (!txbody) continue;
                        for (pugi::xml_node p = txbody.first_child(); p;
                             p = p.next_sibling()) {
                            if (!EsNodo(p, "p")) continue;
                            ParrafoPptx par = LeerParrafo(p);
                            if (!par.runs.empty())
                                d.parrafos_ricos.push_back(std::move(par));
                        }
                    }
                }
            }
        }

        // Compatibilidad v1: texto plano de cada párrafo rico.
        d.parrafos.reserve(d.parrafos_ricos.size());
        for (const auto& par : d.parrafos_ricos)
            d.parrafos.push_back(par.Texto());

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
