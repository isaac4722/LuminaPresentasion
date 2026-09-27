// src/managed/FusionHP.Managed/Forms/StudioForm.cs — Estudio (editor de programas)

using System;
using System.Windows.Forms;
using FusionHP.Managed.Data;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class StudioForm : Form
    {
        private readonly IpcClient _ipc;
        private AhpProgram _programa;

        public StudioForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            _programa = new AhpProgram { Titulo = "(sin título)" };
            ActualizarTituloVentana();
        }

        private void ActualizarTituloVentana()
        {
            Text = "FUSION-HP — Estudio: " + _programa.Titulo;
        }

        private void nuevoToolStripMenuItem_Click(object sender, EventArgs e)
        {
            _programa = new AhpProgram { Titulo = "Nuevo programa" };
            ActualizarTituloVentana();
            lstEscenarios.Items.Clear();
        }

        private void abrirToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog { Filter = "Programa FUSION-HP (*.ahp)|*.ahp" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                // TODO(P0): IPC programa.abrir { ruta }
                _programa.Titulo = System.IO.Path.GetFileNameWithoutExtension(dlg.FileName);
                ActualizarTituloVentana();
            }
        }

        private void guardarToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dlg = new SaveFileDialog { Filter = "Programa FUSION-HP (*.ahp)|*.ahp" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                // TODO(P0): IPC programa.guardar { ruta }
            }
        }

        private void exportarCSVToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // P1: historial + CSV
            using (var dlg = new SaveFileDialog { Filter = "CSV (*.csv)|*.csv" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                // TODO(P1): exportar historial a CSV
            }
        }

        private void btnAgregarEscenario_Click(object sender, EventArgs e)
        {
            var s = new Scenario { Id = "esc-" + (lstEscenarios.Items.Count + 1).ToString("D3"),
                                   Nombre = "Nuevo escenario" };
            _programa.Escenarios.Add(s);
            lstEscenarios.Items.Add(s.Nombre);
        }
    }
}
