// src/managed/FusionHP.Managed.Tests/AhpFormatTests.cs
// Tests del modelo ahp.v1 (versión gestionada).

using FusionHP.Managed.Data;

namespace FusionHP.Managed.Tests
{
    public static class AhpFormatTests
    {
        [Test]
        public static void Programa_Nuevo_TieneListaEscenariosVacia()
        {
            var p = new AhpProgram { Titulo = "test" };
            Assert(p.Escenarios.Count == 0, "Programa nuevo debe tener 0 escenarios");
        }

        [Test]
        public static void Scenario_AgregarElemento_QuedaEnLaLista()
        {
            var s = new Scenario { Id = "esc-1", Nombre = "Bienvenida" };
            s.Elementos.Add(new Element { Id = "el-1", Tipo = TipoElemento.Texto });
            Assert(s.Elementos.Count == 1, "Escenario debe tener 1 elemento");
            Assert(s.Elementos[0].Id == "el-1", "Elemento debe tener id=el-1");
        }

        [Test]
        public static void Theme_ColorHex_RedondeaBien()
        {
            var c = new TemaColor { R = 0xFF, G = 0xA0, B = 0x40 };
            Assert(c.Hex == "#FFA040", "Hex debe ser #FFA040, fue: " + c.Hex);
            c.Hex = "#012345";
            Assert(c.R == 0x01 && c.G == 0x23 && c.B == 0x45, "Hex debe parsear a 01,23,45");
        }

        private static void Assert(bool condicion, string mensaje)
        {
            if (!condicion) throw new System.Exception(mensaje);
        }
    }
}
