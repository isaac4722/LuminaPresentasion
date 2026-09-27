// src/managed/FusionHP.Managed/Data/AhpProgram.cs
// Modelo de datos del formato ahp.v1, espejo de la estructura C++ del núcleo.

using System;
using System.Collections.Generic;

namespace FusionHP.Managed.Data
{
    public class AhpProgram
    {
        public string Titulo = "";
        public string Fecha = "";
        public string Autor = "";
        public string TemaRaiz = "";
        public string Notas = "";
        public List<Scenario> Escenarios = new List<Scenario>();
    }

    public class Reciente
    {
        public string Ruta = "";
        public int VecesUsado = 0;
    }
}
