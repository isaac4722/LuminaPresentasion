// src/managed/FusionHP.Managed/Forms/StudioForm.cs — Estudio REAL
//
// Antes: nuevo/abrir/guardar fingidos (solo cambiaban el título de la
// ventana). Ahora opera el programa REAL del motor por ipc.v1:
//   - Archivo > Nuevo/Abrir/Guardar: programa.nuevo/abrir/guardar.
//   - La lista muestra el programa actual (programa.estado).
//   - Agregar texto (desde notas), PPTX (lector directo), canto
//     (Biblioteca) y versículo (Biblia rápida); quitar elemento.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class StudioForm : Form
    {
        private readonly IpcClient _ipc;

        // Renglón seleccionado de la lista del programa.
        private sealed class Ref
        {
            public string Escenario = "";
            public string Elemento;   // null = escenario
            public string Titulo = "";
            public override string ToString() { return Titulo; }
        }

        public StudioForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarPrograma();
        }

        private void ActualizarTituloVentana(string titulo)
        {
            Text = "FUSION-HP — Estudio: " +
                   (titulo.Length > 0 ? titulo : "(sin programa)");
        }

        // --- Programa actual ------------------------------------------------------
        private void CargarPrograma()
        {
            lstEscenarios.Items.Clear();

            var r = _ipc.Pedir("programa.estado", "{}");
            var prog = (r != null && IpcJson.Ok(r))
                ? IpcJson.Objeto(r, "programa") : null;

            if (prog == null)
            {
                lstEscenarios.Items.Add(new Ref
                { Titulo = "(sin programa: crea uno con Archivo > Nuevo)" });
                ActualizarTituloVentana("");
                return;
            }

            var meta = IpcJson.Objeto(prog, "meta");
            string titulo = meta != null ? IpcJson.Texto(meta, "titulo") : "";
            ActualizarTituloVentana(titulo);

            var escenarios = IpcJson.Arreglo(prog, "escenarios");
            if (escenarios == null) return;
            foreach (var itemEsc in escenarios)
            {
                var esc = IpcJson.ObjetoEn(itemEsc);
                if (esc == null) continue;
                string escId = IpcJson.Texto(esc, "id");
                lstEscenarios.Items.Add(new Ref
                {
                    Escenario = escId,
                    Titulo = "▶ " + IpcJson.Texto(esc, "nombre")
                });
                var elementos = IpcJson.Arreglo(esc, "elementos");
                if (elementos == null) continue;
                foreach (var itemEl in elementos)
                {
                    var el = IpcJson.ObjetoEn(itemEl);
                    if (el == null) continue;
                    string tipo = IpcJson.Texto(el, "tipo", "texto");
                    int diapo = IpcJson.Entero(el, "diapositiva");
                    string etiqueta = tipo == "pptx" && diapo > 0
                        ? "     [" + tipo + " " + diapo + "] "
                        : "     [" + tipo + "] ";
                    lstEscenarios.Items.Add(new Ref
                    {
                        Escenario = escId,
                        Elemento = IpcJson.Texto(el, "id"),
                        Titulo = etiqueta + IpcJson.Texto(el, "titulo")
                    });
                }
            }
        }

        // --- Archivo -----------------------------------------------------------------
        private void nuevoToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var r = _ipc.Pedir("programa.nuevo",
                               "{\"titulo\":\"Programa sin título\"}");
            if (r == null || !IpcJson.Ok(r))
            {
                Aviso("No se pudo crear el programa:\n" +
                      IpcJson.MensajeError(r));
                return;
            }
            CargarPrograma();
        }

        private void abrirToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog
            {
                Filter = "Programa FUSION-HP (*.ahp)|*.ahp|" +
                         "Todos los archivos (*.*)|*.*"
            })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                string payload = "{\"ruta\":\"" +
                    dlg.FileName.Replace("\\", "\\\\")
                                .Replace("\"", "\\\"") + "\"}";
                var r = _ipc.Pedir("programa.abrir", payload);
                if (r == null || !IpcJson.Ok(r))
                {
                    Aviso("No se pudo abrir el programa:\n" +
                          IpcJson.MensajeError(r));
                    return;
                }
                CargarPrograma();
            }
        }

        private void guardarToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dlg = new SaveFileDialog
            {
                Filter = "Programa FUSION-HP (*.ahp)|*.ahp",
                DefaultExt = "ahp"
            })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                string payload = "{\"ruta\":\"" +
                    dlg.FileName.Replace("\\", "\\\\")
                                .Replace("\"", "\\\"") + "\"}";
                var r = _ipc.Pedir("programa.guardar", payload);
                if (r == null || !IpcJson.Ok(r))
                {
                    Aviso("No se pudo guardar:\n" + IpcJson.MensajeError(r));
                    return;
                }
                CargarPrograma();
            }
        }

        private void exportarCSVToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Exporta el programa actual (ahp) a CSV plano: escenario,
            // elemento, tipo. Útil para imprimir el orden del culto.
            using (var dlg = new SaveFileDialog
            { Filter = "CSV (*.csv)|*.csv", DefaultExt = "csv" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                var lineas = new List<string> { "escenario;elemento;tipo;titulo" };
                var r = _ipc.Pedir("programa.estado", "{}");
                var prog = (r != null && IpcJson.Ok(r))
                    ? IpcJson.Objeto(r, "programa") : null;
                if (prog != null)
                {
                    var escenarios = IpcJson.Arreglo(prog, "escenarios");
                    if (escenarios != null)
                    {
                        foreach (var itemEsc in escenarios)
                        {
                            var esc = IpcJson.ObjetoEn(itemEsc);
                            if (esc == null) continue;
                            string escNombre = IpcJson.Texto(esc, "nombre");
                            var elementos = IpcJson.Arreglo(esc, "elementos");
                            if (elementos == null) continue;
                            foreach (var itemEl in elementos)
                            {
                                var el = IpcJson.ObjetoEn(itemEl);
                                if (el == null) continue;
                                lineas.Add(escNombre.Replace(';', ' ') + ";" +
                                           IpcJson.Texto(el, "tipo") + ";" +
                                           IpcJson.Texto(el, "titulo")
                                                   .Replace(';', ' '));
                            }
                        }
                    }
                }
                try
                {
                    System.IO.File.WriteAllLines(dlg.FileName, lineas);
                }
                catch (Exception ex)
                {
                    Aviso("No se pudo escribir el CSV:\n" + ex.Message);
                }
            }
        }

        // --- Agregar contenido ---------------------------------------------------------
        private void btnAgregarEscenario_Click(object sender, EventArgs e)
        {
            // Agrega un elemento de TEXTO con lo escrito en las notas.
            var lineas = new List<string>();
            foreach (string l in txtNotas.Lines)
            {
                string t = l.Trim();
                if (t.Length > 0 && !t.StartsWith("Notas del", StringComparison.Ordinal))
                    lineas.Add(t);
            }
            if (lineas.Count == 0)
            {
                Aviso("Escribe el texto en las notas de abajo; cada línea " +
                      "será un renglón en pantalla.");
                return;
            }
            var parts = new List<string>();
            foreach (string l in lineas)
                parts.Add("\"" + l.Replace("\\", "\\\\")
                                  .Replace("\"", "\\\"") + "\"");
            var r = _ipc.Pedir("programa.agregar_texto",
                "{\"titulo\":\"Notas\",\"lineas\":[" +
                string.Join(",", parts.ToArray()) + "]}");
            if (r == null || !IpcJson.Ok(r))
            {
                Aviso("No se pudo agregar el texto:\n" +
                      IpcJson.MensajeError(r));
                return;
            }
            CargarPrograma();
        }

        private void agregarCantoToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var f = new LibraryForm(_ipc)) f.ShowDialog(this);
            CargarPrograma();
        }

        private void agregarVersiculoToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var f = new BibleQuickForm(_ipc)) f.ShowDialog(this);
            CargarPrograma();
        }

        private void agregarPptxToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog
            {
                Filter = "Presentación PowerPoint (*.pptx;*.pptm)|*.pptx;*.pptm",
                Title = "Agregar presentación (modo directo, sin PowerPoint)"
            })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                string payload = "{\"ruta\":\"" +
                    dlg.FileName.Replace("\\", "\\\\")
                                .Replace("\"", "\\\"") + "\"}";
                var r = _ipc.Pedir("programa.agregar_pptx", payload);
                if (r == null || !IpcJson.Ok(r))
                {
                    Aviso("No se pudo leer el paquete pptx:\n" +
                          IpcJson.MensajeError(r));
                    return;
                }
                int n = IpcJson.Entero(r, "diapositivas");
                Aviso("Presentación agregada: " + n + " diapositivas\n" +
                      "(un elemento por diapositiva, modo directo).");
                CargarPrograma();
            }
        }

        private void quitarElementoToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var sel = lstEscenarios.SelectedItem as Ref;
            if (sel == null || sel.Elemento == null)
            {
                Aviso("Selecciona un ELEMENTO (renglón sangrado) para quitar.");
                return;
            }
            var r = _ipc.Pedir("programa.quitar_elemento",
                "{\"escenario_id\":\"" + sel.Escenario +
                "\",\"elemento_id\":\"" + sel.Elemento + "\"}");
            if (r == null || !IpcJson.Ok(r))
            {
                Aviso("No se pudo quitar:\n" + IpcJson.MensajeError(r));
                return;
            }
            CargarPrograma();
        }

        private void Aviso(string msg)
        {
            MessageBox.Show(this, msg, "FUSION-HP — Estudio",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Information);
        }
    }
}
