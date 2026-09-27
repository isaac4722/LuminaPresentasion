// src/managed/FusionHP.Managed/Data/Scenario.cs

using System.Collections.Generic;

namespace FusionHP.Managed.Data
{
    public class Scenario
    {
        public string Id = "";
        public string Nombre = "";
        public string Notas = "";
        public string Tema = "";
        // Color solido del escenario ("#RRGGBB"/"#RGB") o vacio =
        // hereda. Destino del "fondo del diseno" al importar PPTX.
        public string FondoColor = "";
        public List<Element> Elementos = new List<Element>();
    }
}
