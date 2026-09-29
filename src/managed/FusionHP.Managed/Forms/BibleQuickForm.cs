// src/managed/FusionHP.Managed/Forms/BibleQuickForm.cs — Biblia rápida (G) REAL
//
// Antes: "(consultando al núcleo...)" era un mensaje FALSO: nunca
// consultaba. Ahora biblia.obtener devuelve el texto de verdad y los
// botones agregan el versículo al programa del motor y lo proyectan.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class BibleQuickForm : Form
    {
        private readonly IpcClient _ipc;
        private string _ultimaCita = "";

        public BibleQuickForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
        }

        private void btnIr_Click(object sender, EventArgs e)
        {
            string cita = txtCita.Text.Trim();
            if (string.IsNullOrEmpty(cita)) return;

            string payload = "{\"cita\":\"" + Escapar(cita) + "\"}";
            var r = _ipc.Pedir("biblia.obtener", payload);
            if (r != null && IpcJson.Ok(r))
            {
                _ultimaCita = cita;
                txtResultado.Text = cita + "\r\n\r\n" +
                                    IpcJson.Texto(r, "texto");
            }
            else
            {
                txtResultado.Text = "No se encontró \"" + cita + "\":\r\n" +
                                    IpcJson.MensajeError(r) +
                                    "\r\n\r\nEjemplos: Juan 3:16, Salmo 23:1-6, Romanos 8:28";
            }
        }

        private void btnFavorito_Click(object sender, EventArgs e)
        {
            string cita = txtCita.Text.Trim();
            if (string.IsNullOrEmpty(cita)) return;
            var r = _ipc.Pedir("biblia.favorito.agregar",
                               "{\"cita\":\"" + Escapar(cita) + "\"}");
            if (r != null && IpcJson.Ok(r))
                txtResultado.AppendText("\r\n\r\n[ favorito agregado: " + cita + " ]");
            else
                txtResultado.AppendText("\r\n\r\n[ fallo favorito: " +
                                        IpcJson.MensajeError(r) + " ]");
        }

        private void btnAgregar_Click(object sender, EventArgs e)
        {
            AgregarAlPrograma(false);
        }

        private void btnTercio_Click(object sender, EventArgs e)
        {
            // Modo Tercio: lo agrega al programa y lo proyecta como lower third.
            if (!AgregarAlPrograma(true)) return;

            _ipc.Enviar("proyeccion.iniciar", "{}");
            Close();
        }

        private bool AgregarAlPrograma(bool tercio)
        {
            string cita = _ultimaCita.Length > 0 ? _ultimaCita : txtCita.Text.Trim();
            if (string.IsNullOrEmpty(cita))
            {
                txtResultado.Text = "Busca primero una cita con \"Ir\".";
                return false;
            }
            string payload = "{\"cita\":\"" + Escapar(cita) + "\"" +
                             (tercio ? ",\"modo\":\"tercio\"" : "") + "}";
            var r = _ipc.Pedir("programa.agregar_versiculo", payload);
            if (r != null && IpcJson.Ok(r))
            {
                txtResultado.AppendText("\r\n\r\n[ agregado al programa: " + cita + " ]");
                return true;
            }
            txtResultado.AppendText("\r\n\r\n[ fallo: " +
                                    IpcJson.MensajeError(r) + " ]");
            return false;
        }

        private void txtCita_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter) { btnIr_Click(sender, e); e.Handled = true; }
        }

        private static string Escapar(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }
    }
}
