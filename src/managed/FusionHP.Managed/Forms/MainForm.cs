// src/managed/FusionHP.Managed/Forms/MainForm.cs — Pantalla de Inicio REAL
//
// Antes mostraba "recientes" inventados y un estado fingido. Ahora TODO
// viene del núcleo por ipc.v1: recientes reales, abrir programa .ahp y
// estado del motor en vivo.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class MainForm : Form
    {
        private readonly IpcClient _ipc;
        // Rutas reales detrás de cada renglón de lstRecientes.
        private readonly List<string> _rutasRecientes = new List<string>();

        public MainForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarRecientes();
            CargarEstado();
        }

        private void CargarRecientes()
        {
            lstRecientes.Items.Clear();
            _rutasRecientes.Clear();

            var r = _ipc.Pedir("programa.recientes", "{}");
            if (r == null)
            {
                lstRecientes.Items.Add("(el núcleo no respondió)");
                return;
            }
            if (!IpcJson.Ok(r))
            {
                lstRecientes.Items.Add("(sin recientes: " +
                                       IpcJson.MensajeError(r) + ")");
                return;
            }

            var arr = IpcJson.Arreglo(r, "recientes");
            if (arr == null || !arr.GetEnumerator().MoveNext())
            {
                lstRecientes.Items.Add("(aún no hay programas abiertos)");
                return;
            }
            foreach (var item in arr)
            {
                string ruta = Convert.ToString(item);
                if (string.IsNullOrEmpty(ruta)) continue;
                _rutasRecientes.Add(ruta);
                lstRecientes.Items.Add(
                    System.IO.Path.GetFileNameWithoutExtension(ruta) +
                    "   —   " + ruta);
            }
        }

        private void CargarEstado()
        {
            var e = _ipc.Pedir("estado.lector", "{}");
            if (e == null)
            {
                lblConfig.Text = "Núcleo: sin respuesta (revisa runtime\\nucleo.log)";
                return;
            }
            string programa = IpcJson.Texto(e, "programa_titulo", "");
            bool visible = IpcJson.Booleano(e, "salida_visible");
            string biblia = IpcJson.Texto(e, "biblia_activa", "(ninguna)");
            string monitor = IpcJson.Texto(e, "monitor_dispositivo", "(auto)");
            lblConfig.Text =
                "Programa: " + (programa.Length > 0 ? programa : "(ninguno)") +
                " · Proyección: " + (visible ? "VISIBLE" : "oculta") +
                " · Biblia: " + biblia +
                " · Monitor: " + monitor;
        }

        private void AbrirPrograma(string ruta)
        {
            if (string.IsNullOrEmpty(ruta) ||
                !System.IO.File.Exists(ruta))
            {
                MessageBox.Show(this, "No existe el archivo:\n" + ruta,
                                "FUSION-HP", MessageBoxButtons.OK,
                                MessageBoxIcon.Warning);
                return;
            }
            string payload = "{\"ruta\":\"" +
                ruta.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"}";
            var r = _ipc.Pedir("programa.abrir", payload);
            if (r != null && IpcJson.Ok(r))
            {
                CargarEstado();
                CargarRecientes();
                using (var f = new StudioForm(_ipc)) f.ShowDialog(this);
            }
            else
            {
                MessageBox.Show(this,
                    "No se pudo abrir el programa:\n" +
                    IpcJson.MensajeError(r),
                    "FUSION-HP", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void btnEstudio_Click(object sender, EventArgs e)
        {
            using (var f = new StudioForm(_ipc)) f.ShowDialog(this);
            CargarEstado();
            CargarRecientes();
        }

        private void btnPresentar_Click(object sender, EventArgs e)
        {
            using (var f = new PresentForm(_ipc)) f.ShowDialog(this);
            CargarEstado();
        }

        private void btnBiblioteca_Click(object sender, EventArgs e)
        {
            using (var f = new LibraryForm(_ipc)) f.ShowDialog(this);
            CargarEstado();
        }

        private void btnEstadoSistema_Click(object sender, EventArgs e)
        {
            using (var f = new DiagnosticForm(_ipc)) f.ShowDialog(this);
            CargarEstado();
        }

        private void btnAbrirPrograma_Click(object sender, EventArgs e)
        {
            using (var dlg = new OpenFileDialog
            {
                Filter = "Programa FUSION-HP (*.ahp)|*.ahp|" +
                         "Todos los archivos (*.*)|*.*",
                Title = "Abrir programa de culto"
            })
            {
                if (dlg.ShowDialog(this) == DialogResult.OK)
                    AbrirPrograma(dlg.FileName);
            }
        }

        private void lstRecientes_DoubleClick(object sender, EventArgs e)
        {
            int i = lstRecientes.SelectedIndex;
            if (i < 0 || i >= _rutasRecientes.Count) return;
            AbrirPrograma(_rutasRecientes[i]);
        }
    }
}
