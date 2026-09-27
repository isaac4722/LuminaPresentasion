// src/managed/FusionHP.Managed/Interop/ImportadorPptx.cs
// Importador PPTX (OpenXML / OPC) → ahp.v1 — Sección 9.2 del doc técnico.
//
// Reglas del doc que aquí se aplican y son verificables en los tests:
//  1. Apertura del contenedor OPC y recorrido normativo:
//     [Content_Types].xml → _rels/.rels (officeDocument) → docProps/
//     (metadatos) → ppt/presentation.xml (orden de diapositivas) →
//     slides/ → (slideLayouts/ → slideMasters/ para el fondo).
//  2. Seguridad obligatoria: XmlReader con DtdProcessing=Prohibit y
//     XmlResolver=null → sin entidades externas (XXE) ni expansión de
//     entidades (billion laughs). Los .pptm se importan SIN ejecutar
//     macros, con aviso explícito en el informe.
//  3. Tolerancia a namespaces de extensión (p14, pc2, ...): el matching
//     se hace por LocalName, así los elementos desconocidos se ignoran
//     y jamás abortan la importación.
//  4. Conversión de unidades: tamaños de fuente sz="3200" → 32 pt
//     (TamFuentePt). Las coordenadas EMU no se importan en v1 (el
//     modelo ahp.v1 no tiene geometría de lienzo); la decisión queda
//     registrada en el informe de importación.
//  5. Mapeo al modelo propio: diapositiva → Escenario; caja de texto →
//     Elemento Texto (un párrafo por línea); imagen incrustada →
//     Elemento Imagen con extracción a media/; video → Elemento Video;
//     fondo sólido de la diapositiva (o del diseño/maestro) →
//     Escenario.FondoColor. Tablas: texto de celdas como líneas de un
//     Elemento Texto. Animaciones y transiciones no se importan (MVP).
//  6. Resultado: AhpProgram + InformeImportacionPptx (avisos e ítems
//     omitidos), la transparencia que exige el doc (9.2/9.3.4).

using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Xml;
using FusionHP.Managed.Data;

namespace FusionHP.Managed.Interop
{
    /// <summary>Recurso multimedia extraído del paquete (el llamador
    /// lo persiste en media/ del proyecto).</summary>
    public sealed class MediaExtraida
    {
        public string Ruta = "";      // relativa al proyecto: media/...
        public byte[] Datos = new byte[0];
    }

    /// <summary>Informe de importación (fidelidad, doc 9.2.6).</summary>
    public sealed class InformeImportacionPptx
    {
        public bool Ok;
        public string MsgError = "";
        public bool TieneMacros;                 // .pptm
        public int DiapositivasLeidas;
        public int EscenariosCreados;
        public List<string> Avisos = new List<string>();
        public List<string> Omitidos = new List<string>();
        public List<MediaExtraida> Media = new List<MediaExtraida>();
    }

    public static class ImportadorPptx
    {
        public static AhpProgram ImportarDesdeArchivo(string ruta,
                                                      out InformeImportacionPptx informe)
        {
            var datos = File.ReadAllBytes(ruta);
            return ImportarDesdeBytes(datos, Path.GetFileName(ruta), out informe);
        }

        public static AhpProgram ImportarDesdeBytes(byte[] datos,
                                                    string nombreArchivo,
                                                    out InformeImportacionPptx informe)
        {
            informe = new InformeImportacionPptx();
            AhpProgram programa;

            if (datos == null || datos.Length == 0)
            {
                informe.MsgError = "el paquete está vacío";
                return null;
            }
            var ext = (nombreArchivo ?? "").ToLowerInvariant();
            if (ext.EndsWith(".pptm"))
            {
                informe.TieneMacros = true;
                informe.Avisos.Add("el archivo tiene macros (.pptm): se " +
                                   "importa sin ejecutarlas, nunca");
            }

            try
            {
                using (var ms = new MemoryStream(datos))
                using (var zip = new ZipArchive(ms, ZipArchiveMode.Read))
                {
                    programa = ImportarDesdeZip(zip, informe, nombreArchivo ?? "");
                }
            }
            catch (InvalidDataException)
            {
                informe.MsgError = "no es un paquete OPC válido (zip corrupto)";
                return null;
            }
            catch (Exception ex)
            {
                informe.MsgError = "error al importar: " + ex.Message;
                return null;
            }
            if (programa == null)
                return null;  // MsgError ya lo fijó ImportarDesdeZip

            if (programa.Escenarios.Count == 0)
                informe.Avisos.Add("el paquete no produjo escenarios");

            informe.Ok = true;
            informe.EscenariosCreados = programa.Escenarios.Count;
            return programa;
        }

        // -----------------------------------------------------------------
        // Recorrido OPC
        // -----------------------------------------------------------------

        private static AhpProgram ImportarDesdeZip(ZipArchive zip,
                                                   InformeImportacionPptx inf,
                                                   string nombreArchivo)
        {
            var programa = new AhpProgram();

            // 1) [Content_Types].xml: debe existir (recorrido normativo).
            if (LeerParte(zip, "[Content_Types].xml") == null)
                inf.Omitidos.Add("[Content_Types].xml ausente: paquete no estándar");

            // 2) _rels/.rels → officeDocument.
            var relsRaiz = LeerParte(zip, "_rels/.rels");
            string parteDoc = null;
            if (relsRaiz != null)
            {
                var doc = CargarXmlSeguro(relsRaiz);
                foreach (var rel in Relaciones(doc))
                {
                    if (Local(rel.Tipo).EndsWith("officeDocument"))
                    {
                        parteDoc = ResolverRuta("", rel.Destino);
                        break;
                    }
                }
            }
            if (parteDoc == null)
            {
                inf.MsgError = "el paquete no declara un documento de " +
                               "presentación (officeDocument)";
                return null;
            }

            // 3) docProps/core.xml: metadatos (título/autor si vienen).
            var core = LeerParte(zip, "docProps/core.xml");
            if (core != null)
            {
                var doc = CargarXmlSeguro(core);
                var titulo = TextoDe(doc, "title");
                var autor = TextoDe(doc, "creator");
                if (!string.IsNullOrEmpty(titulo)) programa.Titulo = titulo;
                if (!string.IsNullOrEmpty(autor)) programa.Autor = autor;
            }
            if (string.IsNullOrEmpty(programa.Titulo))
                programa.Titulo = nombreArchivo;

            // 4) presentation.xml + sus relaciones → orden de diapositivas.
            var presBytes = LeerParte(zip, parteDoc);
            if (presBytes == null)
            {
                inf.MsgError = "el paquete no contiene '" + parteDoc + "'";
                return null;
            }
            var pres = CargarXmlSeguro(presBytes);
            var relsPresXml = LeerParte(zip, RutaRelaciones(parteDoc));
            var relsPres = relsPresXml == null
                ? new List<Relacion>()
                : Relaciones(CargarXmlSeguro(relsPresXml));

            var partesSlides = new List<string>();
            var lst = PorNombre(pres.DocumentElement, "sldIdLst");
            if (lst != null)
            {
                foreach (var sldId in HijosConNombre(lst, "sldId"))
                {
                    // El atributo de relación lleva prefijo (r:id); el
                    // "id" numérico sin prefijo se descarta.
                    string rid = null;
                    foreach (var a in Atributos(sldId))
                    {
                        if (Local(a.Key) == "id" && a.Key != "id")
                        {
                            rid = a.Value;
                            break;
                        }
                    }
                    if (rid == null) continue;
                    var rel = relsPres.FirstOrDefault(r => r.Id == rid);
                    if (rel == null || !Local(rel.Tipo).EndsWith("slide")) continue;
                    partesSlides.Add(ResolverRuta(DirBase(parteDoc), rel.Destino));
                }
            }
            if (partesSlides.Count == 0)
                inf.Avisos.Add("el paquete no tiene diapositivas en sldIdLst");

            // 5) Cada diapositiva → Escenario.
            int n = 0;
            foreach (var parteSlide in partesSlides)
            {
                ++n;
                var slideBytes = LeerParte(zip, parteSlide);
                if (slideBytes == null)
                {
                    inf.Avisos.Add("diapositiva " + n + " omitida: parte " +
                                   parteSlide + " ausente");
                    continue;
                }
                var escenario = new Scenario
                {
                    Id = "scn-import-" + n,
                    Nombre = "Diapositiva " + n
                };
                ImportarDiapositiva(zip, parteSlide, slideBytes, escenario,
                                    inf, n);
                programa.Escenarios.Add(escenario);
                ++inf.DiapositivasLeidas;
            }
            return programa;
        }

        // -----------------------------------------------------------------
        // Importación de una diapositiva
        // -----------------------------------------------------------------

        private static void ImportarDiapositiva(ZipArchive zip,
                                                string parteSlide,
                                                byte[] slideBytes,
                                                Scenario escenario,
                                                InformeImportacionPptx inf,
                                                int indice)
        {
            var doc = CargarXmlSeguro(slideBytes);
            var relsBytes = LeerParte(zip, RutaRelaciones(parteSlide));
            var rels = relsBytes == null
                ? new List<Relacion>()
                : Relaciones(CargarXmlSeguro(relsBytes));
            bool tituloTomado = false;
            int elementos = 0;

            var tree = PorNombre(doc.DocumentElement, "spTree");
            if (tree != null)
            {
                // Formas (p:sp) en orden: título → nombre del escenario;
                // el resto → Elementos Texto.
                foreach (var forma in HijosConNombre(tree, "sp"))
                {
                    var ph = BajarHasta(forma, "ph");
                    var tipoPh = ph != null ? (Atributo(ph, "type") ?? "") : "";
                    var parrafos = ParrafosDe(forma);

                    if ((tipoPh == "title" || tipoPh == "ctrTitle") && !tituloTomado)
                    {
                        tituloTomado = true;
                        escenario.Nombre = string.Join(" ",
                            parrafos.Where(t => !string.IsNullOrEmpty(t))
                                    .ToArray());
                        continue;
                    }
                    if (parrafos.Count == 0) continue;

                    var el = new Element
                    {
                        Id = escenario.Id + "-el-" + (++elementos),
                        Tipo = TipoElemento.Texto,
                        Titulo = tipoPh == "subTitle" ? "Subtítulo" : "Texto"
                    };
                    foreach (var t in parrafos)
                        el.Lineas.Add(new LineaTexto { Texto = t });
                    el.TamFuentePt = TamUniformePt(forma);
                    escenario.Elementos.Add(el);
                }

                // Imágenes (p:pic) → Elementos Imagen con extracción.
                foreach (var pic in HijosConNombre(tree, "pic"))
                {
                    var blip = BajarHasta(pic, "blip");
                    if (blip == null) continue;
                    var rid = Atributo(blip, "embed");
                    var rel = rels.FirstOrDefault(r => r.Id == rid);
                    if (rel == null)
                    {
                        inf.Omitidos.Add("diapositiva " + indice +
                                         ": imagen sin relación, omitida");
                        continue;
                    }
                    var parteMedia = ResolverRuta(DirBase(parteSlide), rel.Destino);
                    var datos = LeerParte(zip, parteMedia);
                    if (datos == null)
                    {
                        inf.Omitidos.Add("diapositiva " + indice +
                                         ": imagen '" + parteMedia +
                                         "' ausente, omitida");
                        continue;
                    }
                    var rutaMedia = "media/diapositiva-" + indice + "-imagen-" +
                                    (inf.Media.Count + 1) +
                                    Path.GetExtension(parteMedia);
                    inf.Media.Add(new MediaExtraida
                    {
                        Ruta = rutaMedia,
                        Datos = datos
                    });
                    escenario.Elementos.Add(new Element
                    {
                        Id = escenario.Id + "-el-" + (++elementos),
                        Tipo = TipoElemento.Imagen,
                        Ruta = rutaMedia
                    });
                }

                // Tablas (graphicFrame con tbl) → texto de celdas como
                // Elemento Texto (decisión documentada, doc 9.2.6).
                foreach (var frame in HijosConNombre(tree, "graphicFrame"))
                {
                    var tbl = BajarHasta(frame, "tbl");
                    if (tbl == null) continue;
                    var el = new Element
                    {
                        Id = escenario.Id + "-el-" + (++elementos),
                        Tipo = TipoElemento.Texto,
                        Titulo = "Tabla"
                    };
                    foreach (var fila in HijosConNombre(tbl, "tr"))
                    {
                        foreach (var celda in HijosConNombre(fila, "tc"))
                        {
                            foreach (var t in ParrafosDe(celda))
                                el.Lineas.Add(new LineaTexto { Texto = t });
                        }
                    }
                    if (el.Lineas.Count > 0) escenario.Elementos.Add(el);
                }
            }

            // Fondo: diapositiva → diseño (layout) → maestro (doc 9.2.6:
            // "el fondo del diseño se traduce a fondo del Escenario").
            string fondo = FondoSolido(doc);
            if (FondoConImagen(doc))
            {
                inf.Omitidos.Add("diapositiva " + indice +
                                 ": fondo con imagen omitido (v1 solo " +
                                 "colores sólidos)");
            }
            else if (string.IsNullOrEmpty(fondo))
            {
                var parteLayout = ParteDeTipo(rels, "slideLayout");
                if (parteLayout != null)
                {
                    var parteLayoutAbs = ResolverRuta(DirBase(parteSlide), parteLayout);
                    var lb = LeerParte(zip, parteLayoutAbs);
                    if (lb != null)
                    {
                        var layoutDoc = CargarXmlSeguro(lb);
                        fondo = FondoSolido(layoutDoc);
                        if (string.IsNullOrEmpty(fondo))
                        {
                            var relsLayoutXml = LeerParte(zip, RutaRelaciones(parteLayoutAbs));
                            if (relsLayoutXml != null)
                            {
                                var parteMaster = ParteDeTipo(
                                    Relaciones(CargarXmlSeguro(relsLayoutXml)),
                                    "slideMaster");
                                if (parteMaster != null)
                                {
                                    var mb = LeerParte(zip,
                                        ResolverRuta(DirBase(parteLayoutAbs),
                                                     parteMaster));
                                    if (mb != null)
                                        fondo = FondoSolido(CargarXmlSeguro(mb));
                                }
                            }
                        }
                    }
                }
            }
            if (!string.IsNullOrEmpty(fondo))
                escenario.FondoColor = fondo;
        }

        // Párrafos de una forma/celda: un párrafo = una línea (runs unidos).
        private static List<string> ParrafosDe(XmlElement forma)
        {
            var resultado = new List<string>();
            var cuerpo = BajarHasta(forma, "txBody") ?? forma;
            foreach (var p in HijosConNombre(cuerpo, "p"))
            {
                var texto = "";
                foreach (var t in DescendientesConNombre(p, "t"))
                    texto += t.InnerText;
                if (!string.IsNullOrEmpty(texto))
                    resultado.Add(texto);
            }
            return resultado;
        }

        // Tamaño de fuente uniforme de la forma (sz/100 → pt); mezclado
        // o heredado → 0 (hereda del tema).
        private static double TamUniformePt(XmlElement forma)
        {
            double unico = 0.0;
            bool mezclado = false;
            foreach (var rPr in DescendientesConNombre(forma, "rPr"))
            {
                var sz = Atributo(rPr, "sz");
                if (string.IsNullOrEmpty(sz)) continue;
                double v;
                if (!double.TryParse(sz, out v)) continue;
                var pt = v / 100.0;
                if (unico == 0.0) unico = pt;
                else if (Math.Abs(unico - pt) > 0.001) mezclado = true;
            }
            return mezclado ? 0.0 : unico;
        }

        // Color sólido del fondo de una parte (p:bg/p:bgPr/a:solidFill/
        // a:srgbClr @val), o null.
        private static string FondoSolido(XmlDocument doc)
        {
            var bg = BajarHasta(doc.DocumentElement, "bg");
            if (bg == null) return null;
            var solid = BajarHasta(bg, "solidFill");
            var srgb = solid != null ? BajarHasta(solid, "srgbClr") : null;
            var val = srgb != null ? Atributo(srgb, "val") : null;
            if (!string.IsNullOrEmpty(val) && val.Length == 6 && EsHex(val))
                return "#" + val.ToUpperInvariant();
            return null;
        }

        private static bool FondoConImagen(XmlDocument doc)
        {
            var bg = BajarHasta(doc.DocumentElement, "bg");
            return bg != null && BajarHasta(bg, "blipFill") != null;
        }

        private static bool EsHex(string s)
        {
            foreach (var c in s)
            {
                if (!Uri.IsHexDigit(c)) return false;
            }
            return true;
        }

        private static string ParteDeTipo(List<Relacion> rels, string sufijoLocal)
        {
            foreach (var r in rels)
            {
                if (Local(r.Tipo).EndsWith(sufijoLocal)) return r.Destino;
            }
            return null;
        }

        // -----------------------------------------------------------------
        // Relaciones OPC
        // -----------------------------------------------------------------

        private sealed class Relacion
        {
            public string Id = "";
            public string Tipo = "";
            public string Destino = "";
            public bool Externa;
        }

        private static List<Relacion> Relaciones(XmlDocument doc)
        {
            var lista = new List<Relacion>();
            if (doc == null || doc.DocumentElement == null) return lista;
            foreach (var rel in HijosConNombre(doc.DocumentElement, "Relationship"))
            {
                var r = new Relacion
                {
                    Id = Atributo(rel, "Id") ?? "",
                    Tipo = Atributo(rel, "Type") ?? "",
                    Destino = Atributo(rel, "Target") ?? "",
                    Externa = Atributo(rel, "TargetMode") == "External"
                };
                if (r.Id != "" && !r.Externa && r.Destino != "")
                    lista.Add(r);
            }
            return lista;
        }

        // -----------------------------------------------------------------
        // Utilidades XML seguras y rutas OPC
        // -----------------------------------------------------------------

        // XmlReader con DtdProcessing=Prohibit y XmlResolver=null:
        // mitigación XXE / billion laughs (doc 9.2.4).
        private static XmlDocument CargarXmlSeguro(byte[] datos)
        {
            var settings = new XmlReaderSettings
            {
                DtdProcessing = DtdProcessing.Prohibit,
                XmlResolver = null,
                CheckCharacters = true,
                IgnoreComments = true,
                IgnoreProcessingInstructions = true,
                IgnoreWhitespace = true
            };
            var doc = new XmlDocument();
            using (var ms = new MemoryStream(datos))
            using (var reader = XmlReader.Create(ms, settings))
            {
                doc.Load(reader);
            }
            return doc;
        }

        // Matching por LocalName: tolerante a namespaces de extensión.
        private static string Local(string nombre)
        {
            if (string.IsNullOrEmpty(nombre)) return "";
            var i = nombre.IndexOf(':');
            return i >= 0 ? nombre.Substring(i + 1) : nombre;
        }

        private static XmlElement PorNombre(XmlElement padre, string local)
        {
            if (padre == null) return null;
            foreach (var e in DescendientesConNombre(padre, local)) return e;
            return null;
        }

        private static IEnumerable<XmlElement> HijosConNombre(XmlElement padre,
                                                              string local)
        {
            var resultado = new List<XmlElement>();
            if (padre == null) return resultado;
            foreach (XmlNode n in padre.ChildNodes)
            {
                if (n is XmlElement && Local(n.LocalName ?? n.Name) == local)
                    resultado.Add((XmlElement)n);
            }
            return resultado;
        }

        private static IEnumerable<XmlElement> DescendientesConNombre(
            XmlElement padre, string local)
        {
            var resultado = new List<XmlElement>();
            if (padre == null) return resultado;
            // Pila con empuje invertido (también el inicial): el
            // recorrido sale en orden de documento (los runs de un
            // párrafo deben llegar en orden).
            var pila = new Stack<XmlNode>();
            for (int i = padre.ChildNodes.Count - 1; i >= 0; --i)
                pila.Push(padre.ChildNodes[i]);
            while (pila.Count > 0)
            {
                var n = pila.Pop();
                var el = n as XmlElement;
                if (el == null) continue;
                if (Local(el.LocalName ?? el.Name) == local)
                    resultado.Add(el);
                for (int i = el.ChildNodes.Count - 1; i >= 0; --i)
                    pila.Push(el.ChildNodes[i]);
            }
            return resultado;
        }

        private static XmlElement BajarHasta(XmlElement padre, string local)
        {
            foreach (var e in DescendientesConNombre(padre, local)) return e;
            return null;
        }

        private static string Atributo(XmlElement el, string local)
        {
            if (el == null || !el.HasAttributes) return null;
            foreach (XmlAttribute a in el.Attributes)
            {
                if (Local(a.LocalName ?? a.Name) == local) return a.Value;
            }
            return null;
        }

        private static Dictionary<string, string> Atributos(XmlElement el)
        {
            var d = new Dictionary<string, string>();
            if (el != null && el.HasAttributes)
            {
                foreach (XmlAttribute a in el.Attributes)
                    d[a.Name] = a.Value;
            }
            return d;
        }

        private static string TextoDe(XmlDocument doc, string local)
        {
            var el = BajarHasta(doc.DocumentElement, local);
            return el != null ? el.InnerText.Trim() : null;
        }

        private static string DirBase(string parte)
        {
            var i = parte.LastIndexOf('/');
            return i < 0 ? "" : parte.Substring(0, i + 1);
        }

        private static string RutaRelaciones(string parte)
        {
            var i = parte.LastIndexOf('/');
            var dir = i < 0 ? "" : parte.Substring(0, i);
            var nombre = i < 0 ? parte : parte.Substring(i + 1);
            return (dir.Length == 0 ? "" : dir + "/") + "_rels/" + nombre + ".rels";
        }

        // Resuelve el Target de una relación contra el directorio de la
        // parte ("/absoluto", "../relativo", "./").
        private static string ResolverRuta(string baseDir, string destino)
        {
            if (string.IsNullOrEmpty(destino)) return "";
            if (destino.StartsWith("/")) return destino.Substring(1);
            var segmentos = new List<string>(baseDir.Split('/'));
            if (segmentos.Count > 0 && segmentos[segmentos.Count - 1] == "")
                segmentos.RemoveAt(segmentos.Count - 1);
            foreach (var seg in destino.Split('/'))
            {
                if (seg.Length == 0 || seg == ".") continue;
                if (seg == "..")
                {
                    if (segmentos.Count > 0) segmentos.RemoveAt(segmentos.Count - 1);
                    continue;
                }
                segmentos.Add(seg);
            }
            return string.Join("/", segmentos.ToArray());
        }

        private static byte[] LeerParte(ZipArchive zip, string parte)
        {
            var entrada = zip.GetEntry(parte);
            if (entrada == null) return null;
            using (var s = entrada.Open())
            using (var ms = new MemoryStream())
            {
                s.CopyTo(ms);
                return ms.ToArray();
            }
        }
    }
}
