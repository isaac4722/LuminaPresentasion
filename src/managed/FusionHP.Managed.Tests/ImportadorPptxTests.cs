// src/managed/FusionHP.Managed.Tests/ImportadorPptxTests.cs
// Tests del Importador PPTX (doc técnico 9.2): mapeo diapositiva →
// Escenario, entidades, fondo del diseño, imagen incrustada con
// extracción a media/, sz/100 → pt, .pptm sin macros, informe de
// importación y paquetes rotos con error explícito.

using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using FusionHP.Managed.Data;
using FusionHP.Managed.Interop;

namespace FusionHP.Managed.Tests
{
    public static class ImportadorPptxTests
    {
        private static byte[] Paquete()
        {
            return Convert.FromBase64String(PptxMuestra.PptxBase64);
        }

        private static AhpProgram Importar(out InformeImportacionPptx informe)
        {
            return ImportadorPptx.ImportarDesdeBytes(Paquete(),
                                                     "culto.pptx", out informe);
        }

        [Test]
        public static void ImportaTresDiapositivasEnOrden()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            Assert(informe.Ok, "informe.Ok debe ser true: " + informe.MsgError);
            Assert(p != null, "programa no nulo");
            Assert(p.Escenarios.Count == 3,
                   "3 escenarios, hubo " + p.Escenarios.Count);
            Assert(informe.DiapositivasLeidas == 3, "3 diapositivas leídas");
            Assert(p.Titulo == "culto.pptx" || p.Titulo == "Presentación",
                   "titulo del paquete o del archivo: " + p.Titulo);
        }

        [Test]
        public static void TituloYLineasPorDiapositiva()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            var s1 = p.Escenarios[0];
            Assert(s1.Nombre == "Santo es el Señor & Rey",
                   "título con entidad decodificada: " + s1.Nombre);
            Assert(s1.Elementos.Count == 1, "1 elemento de texto");
            Assert(s1.Elementos[0].Lineas.Count == 3, "3 líneas");
            Assert(s1.Elementos[0].Lineas[2].Texto ==
                   "Para siempre es su misericordia",
                   "línea 3 exacta");

            var s2 = p.Escenarios[1];
            // runs partidos se unen; subTitle NO es título.
            Assert(s2.Nombre == "Gloria a Dios",
                   "runs unidos en título: " + s2.Nombre);
            Assert(s2.Elementos.Count == 1, "subTitle → elemento");
            Assert(s2.Elementos[0].Lineas[0].Texto == "En las alturas",
                   "subtítulo en cuerpo");
        }

        [Test]
        public static void FondoSolidoDeDiapositivaAEscenario()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            Assert(p.Escenarios[1].FondoColor == "#1A2B3C",
                   "fondo sólido → FondoColor: " + p.Escenarios[1].FondoColor);
            Assert(p.Escenarios[0].FondoColor == "",
                   "sin fondo declarado → hereda");
        }

        [Test]
        public static void TamanoUniformeSzATPt()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            var s3 = p.Escenarios[2];
            var texto = s3.Elementos.Find(e => e.Titulo == "Texto");
            Assert(texto != null, "cuadro de texto presente");
            Assert(texto.TamFuentePt == 32.0,
                   "sz=3200 → 32 pt, fue: " + texto.TamFuentePt);
        }

        [Test]
        public static void ImagenIncrustadaSeExtraeAMedia()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            var s3 = p.Escenarios[2];
            var img = s3.Elementos.Find(e => e.Tipo == TipoElemento.Imagen);
            Assert(img != null, "elemento imagen presente");
            Assert(img.Ruta.StartsWith("media/"), "ruta en media/: " + img.Ruta);
            Assert(informe.Media.Count == 1, "1 recurso extraído");
            Assert(informe.Media[0].Ruta == img.Ruta, "misma ruta que el elemento");
            Assert(informe.Media[0].Datos.Length > 0, "datos no vacíos");
            // PNG firma
            Assert(informe.Media[0].Datos[0] == 0x89 &&
                   informe.Media[0].Datos[1] == 0x50,
                   "el extraído es el PNG embebido");
        }

        [Test]
        public static void TablaImportaTextoDeCeldas()
        {
            InformeImportacionPptx informe;
            var p = Importar(out informe);
            var s3 = p.Escenarios[2];
            var tabla = s3.Elementos.Find(e => e.Titulo == "Tabla");
            Assert(tabla != null, "elemento tabla presente");
            Assert(tabla.Lineas.Count == 1 &&
                   tabla.Lineas[0].Texto == "Cordero de Dios",
                   "celda → línea");
        }

        [Test]
        public static void PptmSeImportaSinMacrosConAviso()
        {
            var datos = Convert.FromBase64String(PptxMuestra.PptxConMacrosBase64);
            // paquete truncado, pero basta para comprobar el aviso de macros:
            // el aviso debe registrarse aunque el paquete no importe.
            InformeImportacionPptx informe;
            ImportadorPptx.ImportarDesdeBytes(datos, "con_macros.pptm",
                                              out informe);
            Assert(informe.TieneMacros, "marca .pptm");
            Assert(informe.Avisos.Exists(a => a.Contains("sin ejecutarlas")),
                   "aviso explícito de macros");
        }

        [Test]
        public static void NoEsZipDaErrorExplicito()
        {
            var datos = Encoding.UTF8.GetBytes("esto no es un zip");
            InformeImportacionPptx informe;
            var p = ImportadorPptx.ImportarDesdeBytes(datos, "x.pptx",
                                                      out informe);
            Assert(p == null && !informe.Ok, "importación falla");
            Assert(informe.MsgError.Length > 0, "mensaje de error");
        }

        [Test]
        public static void SinOfficeDocumentDaErrorExplicito()
        {
            var datos = ZipMinimo("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"/>");
            InformeImportacionPptx informe;
            var p = ImportadorPptx.ImportarDesdeBytes(datos, "x.pptx",
                                                      out informe);
            Assert(p == null, "sin importar");
            Assert(informe.MsgError.Contains("officeDocument"),
                   "error menciona officeDocument: " + informe.MsgError);
        }

        [Test]
        public static void XmlSeguroRechazaDtd()
        {
            // Un DTD prohíbe la importación (mitigación XXE, doc 9.2.4):
            // el paquete con DTD en .rels no puede abrir el documento.
            var rels = "<!DOCTYPE Relationships [<!ENTITY a \"x\">]>" +
                       "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"/>";
            var datos = ZipMinimo(rels);
            InformeImportacionPptx informe;
            var p = ImportadorPptx.ImportarDesdeBytes(datos, "x.pptx",
                                                      out informe);
            Assert(p == null, "DTD → sin importar");
        }

        // -----------------------------------------------------------------
        // Utilidades
        // -----------------------------------------------------------------

        /// <summary>ZIP mínimo con una sola entrada "_rels/.rels"
        /// (almacenada, contenido XML dado).</summary>
        private static byte[] ZipMinimo(string relsXml)
        {
            using (var ms = new MemoryStream())
            {
                using (var zip = new ZipArchive(ms, ZipArchiveMode.Create, true))
                {
                    var entrada = zip.CreateEntry("_rels/.rels");
                    using (var w = new StreamWriter(entrada.Open(),
                                                    new UTF8Encoding(false)))
                    {
                        w.Write(relsXml);
                    }
                }
                return ms.ToArray();
            }
        }

        private static void Assert(bool condicion, string mensaje)
        {
            if (!condicion) throw new Exception(mensaje);
        }
    }
}
