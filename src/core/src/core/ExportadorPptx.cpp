// src/core/src/core/ExportadorPptx.cpp — Escritor de paquetes .pptx
// (ISO/IEC-29500 / OPC) del exportador del núcleo.
//
// Estructura mínima VÁLIDA (PowerPoint la abre sin reparo):
//   [Content_Types].xml
//   _rels/.rels                    → officeDocument
//   ppt/presentation.xml           → sldMasterIdLst + sldIdLst + sldSz
//   ppt/_rels/presentation.xml.rels→ máster, diapositivas y props
//   ppt/presProps.xml, viewProps.xml, tableStyles.xml (vacías)
//   ppt/slideMasters/slideMaster1.xml (+ rels)
//   ppt/slideLayouts/slideLayout1.xml (+ rels)
//   ppt/theme/theme1.xml           (tema mínimo con clr/font/fmtScheme)
//   ppt/slides/slideN.xml (+ rels) → un cuadro de texto con la unidad
//
// Las entradas van almacenadas (método 0): OPC válido; nuestro propio
// LectorPptx relee el paquete y verifica CRC en los tests de round-trip.

#include "fusion/core/Exportador.h"

#include "ImagenesExport.h"
#include "ZipInterno.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace fusion {

namespace {

// ---------------------------------------------------------------------------
// Ayudas XML
// ---------------------------------------------------------------------------

std::string EscaparXml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}

// Las líneas de una unidad tal como se pintan: el texto de los medios
// y de los paquetes pptx es su referencia (en v1 no se incrustan).
void LineasDeUnidad(const UnidadExport& u, std::vector<std::string>* out) {
    if (u.tipo == UnidadExport::Tipo::Texto) {
        // Un '\n' incrustado en la línea se convierte en párrafo propio
        // (no cambia el número de unidades, solo el render).
        for (const auto& l : u.lineas) {
            std::string trozo;
            for (char c : l) {
                if (c == '\n') {
                    out->push_back(trozo);
                    trozo.clear();
                } else {
                    trozo.push_back(c);
                }
            }
            out->push_back(trozo);
        }
        return;
    }
    out->push_back(u.ruta.empty() ? "(sin archivo)" : u.ruta);
}

const char kDecl[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n";

const char kNs[] =
    "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
    "relationships\" "
    "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"";

// Cuadro de texto con párrafos. El primer párrafo es el título (si lo
// hay) y el resto el cuerpo; los avisos van en cursiva pequeña.
// `id` evita colisiones de cNvPr cuando la diapositiva lleva además una
// imagen (p:pic id=2 → texto id=3). off_y/cy permiten la banda superior
// cuando el cuadro acompaña a una imagen a pantalla completa.
std::string CuadroTexto(const std::vector<std::string>& parrafos,
                        int id = 2, int off_y = 609600, int cy = 5638800) {
    std::string xml;
    xml += "<p:sp><p:nvSpPr><p:cNvPr id=\"" + std::to_string(id) +
           "\" name=\"Fusion\"/>";
    xml += "<p:cNvSpPr txBox=\"1\"/><p:nvPr/></p:nvSpPr>";
    xml += "<p:spPr><a:xfrm><a:off x=\"838200\" y=\"" +
           std::to_string(off_y) + "\"/>";
    xml += "<a:ext cx=\"10515600\" cy=\"" + std::to_string(cy) +
           "\"/></a:xfrm>";
    xml += "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr>";
    xml += "<p:txBody><a:bodyPr wrap=\"square\"><a:normAutofit/></a:bodyPr>"
           "<a:lstStyle/>";
    bool primero = true;
    for (const auto& linea : parrafos) {
        if (linea.empty()) {
            xml += "<a:p><a:endParaRPr lang=\"es-ES\" sz=\"2000\"/></a:p>";
            continue;
        }
        if (primero) {
            xml += "<a:p><a:pPr algn=\"ctr\"/><a:r><a:rPr lang=\"es-ES\" "
                   "sz=\"2800\" b=\"1\" dirty=\"0\"/>";
            xml += "<a:t>" + EscaparXml(linea) + "</a:t></a:r></a:p>";
        } else {
            xml += "<a:p><a:pPr algn=\"ctr\"/><a:r><a:rPr lang=\"es-ES\" "
                   "sz=\"2000\" dirty=\"0\"/>";
            xml += "<a:t>" + EscaparXml(linea) + "</a:t></a:r></a:p>";
        }
        primero = false;
    }
    if (primero)  // sin párrafos: uno vacío para no romper el esquema
        xml += "<a:p><a:endParaRPr lang=\"es-ES\" sz=\"2000\"/></a:p>";
    xml += "</p:txBody></p:sp>";
    return xml;
}

// Imagen a pantalla completa (p:pic). `rid` es la relación del blip.
std::string XmlImagen(const std::string& rid) {
    std::string xml;
    xml += "<p:pic><p:nvPicPr><p:cNvPr id=\"2\" name=\"Imagen\"/>"
           "<p:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></p:cNvPicPr>"
           "<p:nvPr/></p:nvPicPr>";
    xml += "<p:blipFill><a:blip r:embed=\"" + rid + "\"/>"
           "<a:stretch><a:fillRect/></a:stretch></p:blipFill>";
    xml += "<p:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/>"
           "<a:ext cx=\"12192000\" cy=\"6858000\"/></a:xfrm>"
           "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr>"
           "</p:pic>";
    return xml;
}

std::string XmlDiapositiva(const std::vector<std::string>& parrafos,
                           const std::string& fondo6 = "",
                           const std::string& pic = "") {
    std::string xml = kDecl;
    xml += "<p:sld " + std::string(kNs) + "><p:cSld><p:spTree>";
    if (!fondo6.empty()) {
        // Fondo sólido del Escenario (herencia 5.4 / doc 9.2.6): lo que
        // el operador fijó para el escenario viaja al paquete exportado.
        xml += "<p:bg><p:bgPr><a:solidFill><a:srgbClr val=\"" + fondo6 +
               "\"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>";
    }
    xml += "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/>"
           "<p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>";
    xml += pic;
    // Con imagen a pantalla completa, el cuadro de texto (solo título)
    // se coloca en la banda superior con otro id.
    xml += pic.empty() ? CuadroTexto(parrafos, 2)
                       : CuadroTexto(parrafos, 3, 0, 914400);
    xml += "</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/>"
           "</p:clrMapOvr></p:sld>";
    return xml;
}

const char kTipoRels[] =
    "http://schemas.openxmlformats.org/package/2006/relationships";
const char kTipoOffice[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "officeDocument";
const char kTipoSlide[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "slide";
const char kTipoMaster[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "slideMaster";
const char kTipoLayout[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "slideLayout";
const char kTipoTema[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "theme";
const char kTipoPresProps[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "presProps";
const char kTipoViewProps[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "viewProps";
const char kTipoTableStyles[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "tableStyles";
const char kTipoImagen[] =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "image";

// FondoHex6 vive en ImagenesExport.h (imgexp), compartida con el PDF.

std::string XmlContentTypes(int n_diapos,
                            const std::vector<std::string>& extensiones) {
    std::string xml = kDecl;
    xml += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/"
           "content-types\">";
    xml += "<Default Extension=\"rels\" ContentType=\"application/vnd."
           "openxmlformats-package.relationships+xml\"/>";
    xml += "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
    // Medios incrustados: un Default por extensión realmente usada.
    for (const auto& ext : extensiones) {
        xml += "<Default Extension=\"" + ext + "\" ContentType=\"" +
               (ext == "png" ? "image/png" : "image/jpeg") + "\"/>";
    }
    xml += "<Override PartName=\"/ppt/presentation.xml\" ContentType="
           "\"application/vnd.openxmlformats-officedocument."
           "presentationml.presentation.main+xml\"/>";
    for (int i = 1; i <= n_diapos; ++i) {
        xml += "<Override PartName=\"/ppt/slides/slide" + std::to_string(i) +
               ".xml\" ContentType=\"application/vnd.openxmlformats-"
               "officedocument.presentationml.slide+xml\"/>";
    }
    xml += "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "presentationml.slideMaster+xml\"/>";
    xml += "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" "
           "ContentType=\"application/vnd.openxmlformats-officedocument."
           "presentationml.slideLayout+xml\"/>";
    xml += "<Override PartName=\"/ppt/theme/theme1.xml\" ContentType="
           "\"application/vnd.openxmlformats-officedocument.theme+xml\"/>";
    xml += "<Override PartName=\"/ppt/presProps.xml\" ContentType="
           "\"application/vnd.openxmlformats-officedocument."
           "presentationml.presProps+xml\"/>";
    xml += "<Override PartName=\"/ppt/viewProps.xml\" ContentType="
           "\"application/vnd.openxmlformats-officedocument."
           "presentationml.viewProps+xml\"/>";
    xml += "<Override PartName=\"/ppt/tableStyles.xml\" ContentType="
           "\"application/vnd.openxmlformats-officedocument."
           "presentationml.tableStyles+xml\"/>";
    xml += "</Types>";
    return xml;
}

std::string XmlPresentation(int n_diapos) {
    std::string xml = kDecl;
    xml += "<p:presentation " + std::string(kNs) + " saveSubsetFonts=\"1\">";
    xml += "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/>"
           "</p:sldMasterIdLst>";
    xml += "<p:sldIdLst>";
    for (int i = 0; i < n_diapos; ++i) {
        xml += "<p:sldId id=\"" + std::to_string(256 + i) + "\" r:id=\"rId" +
               std::to_string(2 + i) + "\"/>";
    }
    xml += "</p:sldIdLst>";
    xml += "<p:sldSz cx=\"12192000\" cy=\"6858000\"/>";  // 16:9
    xml += "<p:notesSz cx=\"6858000\" cy=\"9144000\"/>";
    xml += "</p:presentation>";
    return xml;
}

std::string XmlSlideMaster() {
    std::string xml = kDecl;
    xml += "<p:sldMaster " + std::string(kNs) + "><p:cSld><p:bg><p:bgPr>"
           "<a:solidFill><a:srgbClr val=\"FFFFFF\"/></a:solidFill>"
           "<a:effectLst/></p:bgPr></p:bg><p:spTree>";
    xml += "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/>"
           "<p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld>";
    xml += "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" "
           "accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
           "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
           "hlink=\"hlink\" folHlink=\"folHlink\"/>";
    xml += "<p:sldLayoutIdLst><p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/>"
           "</p:sldLayoutIdLst>";
    xml += "</p:sldMaster>";
    return xml;
}

std::string XmlSlideLayout() {
    std::string xml = kDecl;
    xml += "<p:sldLayout " + std::string(kNs) +
           " type=\"blank\" preserve=\"1\"><p:cSld name=\"En blanco\">"
           "<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/>"
           "<p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree>"
           "</p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>"
           "</p:sldLayout>";
    return xml;
}

std::string XmlTema() {
    std::string xml = kDecl;
    xml += "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/"
           "2006/main\" name=\"Fusion\"><a:themeElements>";
    xml += "<a:clrScheme name=\"Fusion\">"
           "<a:dk1><a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1>"
           "<a:lt1><a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1>"
           "<a:dk2><a:srgbClr val=\"44546A\"/></a:dk2>"
           "<a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2>"
           "<a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1>"
           "<a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2>"
           "<a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3>"
           "<a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4>"
           "<a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5>"
           "<a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6>"
           "<a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink>"
           "<a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink>"
           "</a:clrScheme>";
    xml += "<a:fontScheme name=\"Fusion\">"
           "<a:majorFont><a:latin typeface=\"Calibri Light\"/>"
           "<a:ea typeface=\"\"/><a:cs typeface=\"\"/></a:majorFont>"
           "<a:minorFont><a:latin typeface=\"Calibri\"/>"
           "<a:ea typeface=\"\"/><a:cs typeface=\"\"/></a:minorFont>"
           "</a:fontScheme>";
    xml += "<a:fmtScheme name=\"Fusion\">"
           "<a:fillStyleLst>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "</a:fillStyleLst>"
           "<a:lnStyleLst>"
           "<a:ln w=\"6350\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
           "</a:solidFill></a:ln>"
           "<a:ln w=\"12700\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
           "</a:solidFill></a:ln>"
           "<a:ln w=\"19050\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
           "</a:solidFill></a:ln>"
           "</a:lnStyleLst>"
           "<a:effectStyleLst>"
           "<a:effectStyle><a:effectLst/></a:effectStyle>"
           "<a:effectStyle><a:effectLst/></a:effectStyle>"
           "<a:effectStyle><a:effectLst/></a:effectStyle>"
           "</a:effectStyleLst>"
           "<a:bgFillStyleLst>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
           "</a:bgFillStyleLst>"
           "</a:fmtScheme>";
    xml += "</a:themeElements></a:theme>";
    return xml;
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

bool Exportador::ExportarPptx(const Programa& p, const std::string& ruta_salida,
                              ResultadoExport* out) {
    if (!out) return false;
    *out = ResultadoExport{};

    const PlanExportResultado plan =
        PlanExport::Planificar(p, FormatoExportacion::Pptx);
    if (!plan.ok) {
        out->msg_error = plan.msg_error;
        return false;
    }
    out->avisos.insert(out->avisos.end(), plan.avisos.begin(),
                       plan.avisos.end());

    const std::vector<UnidadExport> unidades =
        PlanExport::EnumerarUnidades(p);

    int con_medio = 0;
    int imagenes_incrustadas = 0;
    std::vector<std::string> xml_diapos;
    std::vector<std::string> rels_diapos;
    std::vector<std::string> partes_media;   // nombre de parte en el zip
    std::vector<std::string> bytes_media;
    std::vector<std::string> extensiones_media;
    xml_diapos.reserve(unidades.size());
    rels_diapos.reserve(unidades.size());

    for (const auto& u : unidades) {
        std::vector<std::string> parrafos;
        std::string pic;
        std::string fondo6;

        // Fondo sólido del Escenario (doc 5.4/9.2.6), si es un color válido.
        if (u.indice_escenario >= 1 &&
            u.indice_escenario <= static_cast<int>(p.escenarios.size())) {
            fondo6 = imgexp::FondoHex6(
                p.escenarios[static_cast<size_t>(u.indice_escenario - 1)]
                    .fondo);
        }

        const bool es_medio = (u.tipo == UnidadExport::Tipo::Medio ||
                               u.tipo == UnidadExport::Tipo::Pptx);
        const imgexp::TipoImagen ti =
            es_medio ? imgexp::TipoPorRuta(u.ruta) : imgexp::TipoImagen::Ninguna;

        if (es_medio && ti != imgexp::TipoImagen::Ninguna) {
            // Imagen PNG/JPEG: se incrusta en el paquete (p:pic).
            std::string bytes;
            if (imgexp::LeerArchivo(u.ruta, &bytes)) {
                const int num = static_cast<int>(partes_media.size()) + 1;
                const std::string ext =
                    ti == imgexp::TipoImagen::Png ? "png" : "jpeg";
                const std::string parte =
                    "ppt/media/image" + std::to_string(num) + "." + ext;
                partes_media.push_back(parte);
                bytes_media.push_back(bytes);
                bool ext_nueva = true;
                for (const auto& e : extensiones_media)
                    if (e == ext) { ext_nueva = false; break; }
                if (ext_nueva) extensiones_media.push_back(ext);
                pic = XmlImagen("rId2");
                ++imagenes_incrustadas;
                // Solo el título en la banda superior (la imagen ya es
                // el contenido).
                if (!u.titulo.empty()) parrafos.push_back(u.titulo);
            } else {
                // Explícito, no silencioso: referencia + aviso.
                if (!u.titulo.empty()) parrafos.push_back(u.titulo);
                parrafos.push_back(u.ruta);
                parrafos.push_back("(imagen no incrustada: no se pudo "
                                   "leer el archivo)");
                out->avisos.push_back("No se pudo leer la imagen '" +
                                      u.ruta + "': va como referencia");
                ++con_medio;
            }
        } else {
            if (!u.titulo.empty()) parrafos.push_back(u.titulo);
            LineasDeUnidad(u, &parrafos);
            if (es_medio) {
                parrafos.push_back(u.tipo == UnidadExport::Tipo::Pptx
                                       ? "(paquete pptx referenciado, "
                                         "no incrustado)"
                                       : "(medio referenciado, "
                                         "no incrustado)");
                ++con_medio;
            }
        }

        xml_diapos.push_back(XmlDiapositiva(parrafos, fondo6, pic));

        // Relaciones de la diapositiva: layout + imagen si la hay (rId2).
        std::string rels = std::string(kDecl) + "<Relationships xmlns=\"" +
                           kTipoRels + "\">";
        rels += std::string("<Relationship Id=\"rId1\" Type=\"") +
                kTipoLayout + "\" Target=\"../slideLayouts/slideLayout1.xml\"/>";
        if (!pic.empty()) {
            rels += std::string("<Relationship Id=\"rId2\" Type=\"") +
                    kTipoImagen + "\" Target=\"../media/image" +
                    std::to_string(partes_media.size()) + "." +
                    extensiones_media.back() + "\"/>";
        }
        rels += "</Relationships>";
        rels_diapos.push_back(rels);
    }
    if (imagenes_incrustadas > 0) {
        out->avisos.push_back(std::to_string(imagenes_incrustadas) +
                              " imagen(es) incrustada(s) en el paquete");
    }
    if (con_medio > 0) {
        out->avisos.push_back(std::to_string(con_medio) +
                              " unidad(es) con medio/pptx referenciado pero "
                              "no incrustado");
    }

    const int n = static_cast<int>(xml_diapos.size());
    zipint::EscritorZip z;
    z.Agregar("[Content_Types].xml", XmlContentTypes(n, extensiones_media));

    std::string rels_raiz = kDecl;
    rels_raiz += "<Relationships xmlns=\"" + std::string(kTipoRels) + "\">";
    rels_raiz += "<Relationship Id=\"rId1\" Type=\"" + std::string(kTipoOffice) +
                 "\" Target=\"ppt/presentation.xml\"/>";
    rels_raiz += "</Relationships>";
    z.Agregar("_rels/.rels", rels_raiz);

    z.Agregar("ppt/presentation.xml", XmlPresentation(n));

    // presentation.xml.rels: rId1 máster, rId2..n+1 diapositivas, y las
    // tres partes accesorias.
    {
        std::string tipos = kDecl;
        tipos += "<Relationships xmlns=\"" + std::string(kTipoRels) + "\">";
        tipos += "<Relationship Id=\"rId1\" Type=\"" + std::string(kTipoMaster) +
                 "\" Target=\"slideMasters/slideMaster1.xml\"/>";
        for (int i = 0; i < n; ++i) {
            tipos += "<Relationship Id=\"rId" + std::to_string(2 + i) +
                     "\" Type=\"" + std::string(kTipoSlide) +
                     "\" Target=\"slides/slide" + std::to_string(1 + i) +
                     ".xml\"/>";
        }
        const int base = 2 + n;
        tipos += "<Relationship Id=\"rId" + std::to_string(base) +
                 "\" Type=\"" + std::string(kTipoPresProps) +
                 "\" Target=\"presProps.xml\"/>";
        tipos += "<Relationship Id=\"rId" + std::to_string(base + 1) +
                 "\" Type=\"" + std::string(kTipoViewProps) +
                 "\" Target=\"viewProps.xml\"/>";
        tipos += "<Relationship Id=\"rId" + std::to_string(base + 2) +
                 "\" Type=\"" + std::string(kTipoTableStyles) +
                 "\" Target=\"tableStyles.xml\"/>";
        tipos += "</Relationships>";
        z.Agregar("ppt/_rels/presentation.xml.rels", tipos);
    }

    z.Agregar("ppt/presProps.xml",
              std::string(kDecl) +
                  "<p:presentationPr " + kNs + "/>");
    z.Agregar("ppt/viewProps.xml",
              std::string(kDecl) + "<p:viewProperties " + kNs + "/>");
    z.Agregar(
        "ppt/tableStyles.xml",
        std::string(kDecl) + "<p:tableStyles " + kNs +
            " def=\"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}\"/>");

    z.Agregar("ppt/theme/theme1.xml", XmlTema());
    z.Agregar("ppt/slideMasters/slideMaster1.xml", XmlSlideMaster());
    z.Agregar("ppt/slideMasters/_rels/slideMaster1.xml.rels",
              std::string(kDecl) + "<Relationships xmlns=\"" +
                  kTipoRels + "\">"
                  "<Relationship Id=\"rId1\" Type=\"" + kTipoLayout +
                  "\" Target=\"../slideLayouts/slideLayout1.xml\"/>"
                  "<Relationship Id=\"rId2\" Type=\"" + kTipoTema +
                  "\" Target=\"../theme/theme1.xml\"/>"
                  "</Relationships>");
    z.Agregar("ppt/slideLayouts/slideLayout1.xml", XmlSlideLayout());
    z.Agregar("ppt/slideLayouts/_rels/slideLayout1.xml.rels",
              std::string(kDecl) + "<Relationships xmlns=\"" +
                  kTipoRels + "\">"
                  "<Relationship Id=\"rId1\" Type=\"" + kTipoMaster +
                  "\" Target=\"../slideMasters/slideMaster1.xml\"/>"
                  "</Relationships>");

    for (int i = 0; i < n; ++i) {
        z.Agregar("ppt/slides/slide" + std::to_string(1 + i) + ".xml",
                  xml_diapos[i]);
        z.Agregar("ppt/slides/_rels/slide" + std::to_string(1 + i) +
                      ".xml.rels",
                  rels_diapos[static_cast<size_t>(i)]);
    }

    // Medios incrustados (ppt/media/imageN.png|jpeg).
    for (size_t m = 0; m < partes_media.size(); ++m) {
        z.Agregar(partes_media[m], bytes_media[m]);
    }

    const std::string paquete = z.Terminar();
    if (paquete.empty()) {
        out->msg_error = "no se pudo construir el paquete zip";
        return false;
    }
    if (!EscribirArchivo(ruta_salida, paquete)) {
        out->msg_error = "no se pudo escribir '" + ruta_salida + "'";
        return false;
    }

    out->archivos.push_back(ruta_salida);
    out->ok = true;
    return true;
}

} // namespace fusion
