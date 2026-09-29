// src/managed/FusionHP.Managed/Ipc/IpcJson.cs — Parseo de respuestas ipc.v1
//
// La consola mostraba DATOS FALSOS ("Libro 1..66", 3 cantos inventados)
// porque jamás parseó las respuestas del núcleo. Este helper convierte el
// JSON del protocolo en diccionarios .NET con conversores tolerantes.
//
// Parser JSON PROPIO y autocontenido (una clase, sin dependencias):
//   - System.Web.Extensions NO resolvió en el runner del CI (y así la
//     capa gestionada queda sin NINGUNA dependencia más allá del
//     framework base, coherente con la regla 100% offline).
//   - Objetos → Dictionary<string,object>; arreglos → object[]; números
//     → long/double; true/false/null nativos.
//   - Malformado → devuelve null (nunca lanza hacia la UI).

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace FusionHP.Managed.Ipc
{
    /// <summary>Parser JSON mínimo (RFC 8259) para respuestas ipc.v1.</summary>
    internal sealed class JsonMini
    {
        private readonly string _s;
        private int _i;

        private JsonMini(string s) { _s = s; }

        public static object Parsear(string json)
        {
            var p = new JsonMini(json);
            p.Espacio();
            object v = p.Valor();
            p.Espacio();
            if (p._i < p._s.Length)
                throw new FormatException("contenido sobrante en el JSON");
            return v;
        }

        private void Espacio()
        {
            while (_i < _s.Length)
            {
                char c = _s[_i];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++_i;
                else break;
            }
        }

        private char Mirar()
        {
            if (_i >= _s.Length) throw new FormatException("JSON truncado");
            return _s[_i];
        }

        private void Esperar(char c)
        {
            if (_i >= _s.Length || _s[_i] != c)
                throw new FormatException("se esperaba '" + c + "' en " + _i);
            ++_i;
        }

        private object Valor()
        {
            char c = Mirar();
            switch (c)
            {
                case '{': return Objeto();
                case '[': return Arreglo();
                case '"': return Cadena();
                case 't': Palabra("true"); return (object)true;
                case 'f': Palabra("false"); return (object)false;
                case 'n': Palabra("null"); return null;
                default: return Numero();
            }
        }

        private void Palabra(string w)
        {
            if (_i + w.Length > _s.Length ||
                string.CompareOrdinal(_s, _i, w, 0, w.Length) != 0)
                throw new FormatException("literal inválido en " + _i);
            _i += w.Length;
        }

        private Dictionary<string, object> Objeto()
        {
            Esperar('{');
            var d = new Dictionary<string, object>();
            Espacio();
            if (Mirar() == '}') { ++_i; return d; }
            while (true)
            {
                Espacio();
                string clave = Cadena();
                Espacio();
                Esperar(':');
                Espacio();
                d[clave] = Valor();
                Espacio();
                char c = Mirar();
                if (c == ',') { ++_i; continue; }
                if (c == '}') { ++_i; return d; }
                throw new FormatException("objeto malformado en " + _i);
            }
        }

        private object[] Arreglo()
        {
            Esperar('[');
            var l = new List<object>();
            Espacio();
            if (Mirar() == ']') { ++_i; return l.ToArray(); }
            while (true)
            {
                Espacio();
                l.Add(Valor());
                Espacio();
                char c = Mirar();
                if (c == ',') { ++_i; continue; }
                if (c == ']') { ++_i; return l.ToArray(); }
                throw new FormatException("arreglo malformado en " + _i);
            }
        }

        private string Cadena()
        {
            Esperar('"');
            var sb = new StringBuilder();
            while (true)
            {
                if (_i >= _s.Length) throw new FormatException("cadena sin cerrar");
                char c = _s[_i++];
                if (c == '"') return sb.ToString();
                if (c == '\\')
                {
                    if (_i >= _s.Length) throw new FormatException("escape sin cierre");
                    char e = _s[_i++];
                    switch (e)
                    {
                        case '"': sb.Append('"'); break;
                        case '\\': sb.Append('\\'); break;
                        case '/': sb.Append('/'); break;
                        case 'b': sb.Append('\b'); break;
                        case 'f': sb.Append('\f'); break;
                        case 'n': sb.Append('\n'); break;
                        case 'r': sb.Append('\r'); break;
                        case 't': sb.Append('\t'); break;
                        case 'u':
                            if (_i + 4 > _s.Length)
                                throw new FormatException("\\u truncado");
                            sb.Append((char)int.Parse(
                                _s.Substring(_i, 4), NumberStyles.HexNumber,
                                CultureInfo.InvariantCulture));
                            _i += 4;
                            break;
                        default:
                            throw new FormatException("escape inválido: \\" + e);
                    }
                }
                else sb.Append(c);
            }
        }

        private object Numero()
        {
            int inicio = _i;
            if (_i < _s.Length && (_s[_i] == '-' || _s[_i] == '+')) ++_i;
            bool es_real = false;
            while (_i < _s.Length)
            {
                char c = _s[_i];
                if (c >= '0' && c <= '9') { ++_i; continue; }
                if (c == '.' || c == 'e' || c == 'E' ||
                    c == '-' || c == '+')   // exponente con signo
                {
                    if (c == '.' || c == 'e' || c == 'E') es_real = true;
                    ++_i;
                    continue;
                }
                break;
            }
            string num = _s.Substring(inicio, _i - inicio);
            if (num.Length == 0) throw new FormatException("número vacío");
            if (!es_real)
            {
                long l;
                if (long.TryParse(num, NumberStyles.Integer,
                                  CultureInfo.InvariantCulture, out l))
                    return l;
            }
            double d;
            if (double.TryParse(num, NumberStyles.Float,
                                CultureInfo.InvariantCulture, out d))
                return d;
            throw new FormatException("número inválido: " + num);
        }
    }

    public static class IpcJson
    {
        /// Respuesta cruda → diccionario (null si no hay nada útil).
        public static Dictionary<string, object> Respuesta(string raw)
        {
            if (string.IsNullOrEmpty(raw)) return null;
            try
            {
                return JsonMini.Parsear(raw) as Dictionary<string, object>;
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
                CultureInfo.InvariantCulture);
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

        /// Arreglo de la clave (object[] del parser propio).
        public static object[] Arreglo(Dictionary<string, object> d,
                                       string clave)
        {
            if (d == null || !d.ContainsKey(clave)) return null;
            return d[clave] as object[];
        }

        public static Dictionary<string, object> Objeto(
            Dictionary<string, object> d, string clave)
        {
            if (d == null || !d.ContainsKey(clave)) return null;
            return d[clave] as Dictionary<string, object>;
        }

        /// Elemento de un arreglo (el parser produce diccionarios).
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
