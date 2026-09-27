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
        public List<Element> Elementos = new List<Element>();
    }
}
