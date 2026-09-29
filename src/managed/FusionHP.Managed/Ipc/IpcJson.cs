// src/managed/FusionHP.Managed/Ipc/IpcJson.cs — Parseo de respuestas ipc.v1
//
// La consola mostraba DATOS FALSOS ("Libro 1..66", 3 cantos inventados)
// porque jamás parseó las respuestas del núcleo. Este helper convierte el
// JSON del protocolo en diccionarios .NET con conversores tolerantes.
//
// SIN NuGet (regla del producto: cero descargas): JavaScriptSerializer
// viene de System.Web.Extensions, ensamblado del propio framework.

using System;
using System.Collections;
using System.Collections.Generic;

namespace FusionHP.Managed.Ipc
{
    public static class IpcJson
    {
        private static readonly JavaScriptSerializer _ser =
            new JavaScriptSerializer { MaxJsonLength = int.MaxValue };

        /// Respuesta cruda → diccionario (null si no hay nada útil).
        public static Dictionary<string, object> Respuesta(string raw)
        {
            if (string.IsNullOrEmpty(raw)) return null;
            try
            {
                return _ser.Deserialize<Dictionary<string, object>>(raw);
            }
            catch
            {
                return null;
            }
        }

        /// ¿La respuesta vino con ok=true?
        public static bool Ok(Dictionary<string, object> r)
        {
            return r != null && Booleano(r, "ok");
        }

        /// Extrae error.code / error.message de una respuesta fallida.
        public static string MensajeError(Dictionary<string, object> r)
        {
            if (r == null) return "(sin respuesta del núcleo)";
            if (Ok(r)) return "";
            var err = Objeto(r, "error");
            if (err == null) return "(error sin detalle)";
            string code = Texto(err, "code", "?");
            string msg = Texto(err, "message", "");
            return "[" + code + "] " + msg;
        }

        public static string Texto(Dictionary<string, object> d, string clave,
                                   string def = "")
        {
            if (d == null || !d.ContainsKey(clave) || d[clave] == null)
                return def;
            return Convert.ToString(d[clave],
                System.Globalization.CultureInfo.InvariantCulture);
        }

        public static int Entero(Dictionary<string, object> d, string clave,
                                 int def = 0)
        {
            if (d == null || !d.ContainsKey(clave) || d[clave] == null)
                return def;
            try { return Convert.ToInt32(d[clave]); }
            catch { return def; }
        }

        public static bool Booleano(Dictionary<string, object> d, string clave,
                                    bool def = false)
        {
            if (d == null || !d.ContainsKey(clave) || d[clave] == null)
                return def;
            try { return Convert.ToBoolean(d[clave]); }
            catch { return def; }
        }

        /// Arreglo de la clave, tolerante a object[] y ArrayList.
        public static IEnumerable Arreglo(Dictionary<string, object> d,
                                          string clave)
        {
            if (d == null || !d.ContainsKey(clave)) return null;
            return d[clave] as IEnumerable;
        }

        public static Dictionary<string, object> Objeto(
            Dictionary<string, object> d, string clave)
        {
            if (d == null || !d.ContainsKey(clave)) return null;
            return d[clave] as Dictionary<string, object>;
        }

        /// Elemento de un arreglo (JavaScriptSerializer da diccionarios).
        public static Dictionary<string, object> ObjetoEn(object item)
        {
            return item as Dictionary<string, object>;
        }
    }

    /// Cliente con respuesta parseada: evita repetir Enviar+Respuesta.
    public static class IpcExt
    {
        public static Dictionary<string, object> Pedir(this IpcClient ipc,
                                                       string tipo,
                                                       string payloadJson)
        {
            return IpcJson.Respuesta(ipc.Enviar(tipo, payloadJson));
        }
    }
}
