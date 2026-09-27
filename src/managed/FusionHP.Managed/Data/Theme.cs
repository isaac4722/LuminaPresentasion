// src/managed/FusionHP.Managed/Data/Theme.cs
// Esquema de tema con herencia en cascada de 4 niveles:
//   Tema → Plantilla → Escenario → Elemento
// Aplicable en caliente al elemento o a todo.

using System.Collections.Generic;

namespace FusionHP.Managed.Data
{
    public class TemaColor
    {
        public byte R, G, B;
        public string Hex
        {
            get { return "#" + R.ToString("X2") + G.ToString("X2") + B.ToString("X2"); }
            set
            {
                if (string.IsNullOrEmpty(value) || value.Length != 7 || value[0] != '#') return;
                R = System.Convert.ToByte(value.Substring(1, 2), 16);
                G = System.Convert.ToByte(value.Substring(3, 2), 16);
                B = System.Convert.ToByte(value.Substring(5, 2), 16);
            }
        }
    }

    public class EstiloTexto
    {
        public string Familia = "Outfit";
        public float Tamano = 60f;
        public TemaColor Color = new TemaColor { R = 255, G = 255, B = 255 };
        public bool Negrita = false;
        public bool Cursiva = false;
        public bool AlineacionCentro = true;
    }

    public class Fondo
    {
        public string Tipo = "solido";   // solido | gradiente | imagen
        public TemaColor Color1 = new TemaColor();
        public TemaColor Color2 = new TemaColor();
        public string RutaImagen = "";
        public string Ajuste = "cubrir";
    }

    public class Theme
    {
        public string Id = "";
        public string Nombre = "";
        public Fondo Fondo = new Fondo();
        public EstiloTexto EstiloTitulo = new EstiloTexto();
        public EstiloTexto EstiloCuerpo = new EstiloTexto();
        public EstiloTexto EstiloLower = new EstiloTexto();
        // Herencia: si un campo está en null, se hereda del nivel superior.
        public string HeredaDe = "";
    }

    // Informe de fidelidad de la herencia (P2).
    public class InformeFidelidad
    {
        public List<string> CamposSobreescritos = new List<string>();
        public List<string> CamposHeredados = new List<string>();
        public int NivelesRecorridos = 0;
    }
}
