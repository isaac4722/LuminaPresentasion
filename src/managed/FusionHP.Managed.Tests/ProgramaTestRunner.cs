// src/managed/FusionHP.Managed.Tests/ProgramaTestRunner.cs
// Arnés de pruebas propio (sin vstest). Carga y ejecuta todas las clases
// con métodos [Test] y reporta verde/rojo.

using System;
using System.Collections.Generic;
using System.Reflection;

namespace FusionHP.Managed.Tests
{
    public static class ProgramaTestRunner
    {
        [STAThread]
        public static int Main(string[] args)
        {
            int ok = 0, fail = 0;
            var asm = Assembly.GetExecutingAssembly();
            foreach (var t in asm.GetTypes())
            {
                foreach (var m in t.GetMethods(BindingFlags.Public | BindingFlags.Static | BindingFlags.Instance))
                {
                    if (m.GetCustomAttributes(typeof(TestAttribute), false).Length == 0) continue;
                    try
                    {
                        object instance = null;
                        if (!m.IsStatic) instance = Activator.CreateInstance(t);
                        m.Invoke(instance, null);
                        Console.ForegroundColor = ConsoleColor.Green;
                        Console.WriteLine("[ OK ] " + t.Name + "." + m.Name);
                        ++ok;
                    }
                    catch (Exception ex)
                    {
                        Console.ForegroundColor = ConsoleColor.Red;
                        Console.WriteLine("[FAIL] " + t.Name + "." + m.Name + " — " +
                                          (ex.InnerException != null ? ex.InnerException.Message : ex.Message));
                        ++fail;
                    }
                }
            }
            Console.ResetColor();
            Console.WriteLine();
            Console.WriteLine("Tests gestionados: " + ok + " OK, " + fail + " FAIL");
            return fail == 0 ? 0 : 1;
        }
    }

    [AttributeUsage(AttributeTargets.Method)]
    public sealed class TestAttribute : Attribute { }
}
