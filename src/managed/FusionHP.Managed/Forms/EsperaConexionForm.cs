// src/managed/FusionHP.Managed/Forms/EsperaConexionForm.cs —
// Ventana de espera "Conectando con el núcleo…".
//
// Antes el arranque podía terminar en un MessageBox de error si el núcleo
// tardaba unos segundos (primer arranque: siembra de la biblia). Ahora la
// conexión es paciente y VISIBLE: esta ventana muestra el progreso, se
// puede cancelar, y solo si el usuario cancela o el núcleo muere se ve el
// error con detalle.

using System;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace FusionHP.Managed.Forms
{
    /// <summary>
    /// Ventana modal de espera mientras el cliente IPC intenta conectar
    /// con FusionCore.exe. La conexión corre en un hilo aparte para que la
    /// ventana pinte y el botón "Cancelar" responda.
    /// </summary>
    public sealed class EsperaConexionForm : Form
    {
        private readonly Ipc.IpcClient _ipc;
        private readonly Label _etiqueta;
        private readonly ProgressBar _progreso;
        private readonly Button _btnCancelar;
        private Thread _hilo;
        private bool _resultado;
        private volatile bool _terminado;

        public EsperaConexionForm(Ipc.IpcClient ipc)
        {
            _ipc = ipc;

            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterScreen;
            ControlBox = false;         // se sale con "Cancelar", no con la X
            ClientSize = new Size(380, 118);
            Text = "FUSION-HP";

            _etiqueta = new Label
            {
                Text = "Conectando con el núcleo de FUSION-HP…",
                AutoSize = false,
                Bounds = new Rectangle(16, 14, 348, 22),
                TextAlign = ContentAlignment.MiddleLeft
            };

            _progreso = new ProgressBar
            {
                Bounds = new Rectangle(16, 42, 348, 18),
                Style = ProgressBarStyle.Marquee,
                MarqueeAnimationSpeed = 40
            };

            _btnCancelar = new Button
            {
                Text = "Cancelar",
                Bounds = new Rectangle(281, 74, 83, 27)
            };
            _btnCancelar.Click += (s, e) => { Cancelar(); };

            Controls.Add(_etiqueta);
            Controls.Add(_progreso);
            Controls.Add(_btnCancelar);
        }

        /// <summary>Resultado de la conexión tras cerrar el diálogo.</summary>
        public bool Conectado { get { return _resultado; } }

        /// <summary>El usuario canceló la espera.</summary>
        public bool Cancelado { get; private set; }

        /// <summary>
        /// Arranca el hilo de conexión y muestra el diálogo (bloqueante).
        /// </summary>
        public new void ShowDialog()
        {
            _hilo = new Thread(IntentarConectar);
            _hilo.IsBackground = true;
            _hilo.Start();

            // Sondeo del resultado desde el temporizador del hilo de UI:
            // sin Invoke cruzado y sin congelar la ventana.
            // (System.Windows.Forms.Timer explícito: System.Threading.Timer
            // es ambiguo con ambos using presentes.)
            var reloj = new System.Windows.Forms.Timer { Interval = 150 };
            reloj.Tick += (s, e) =>
            {
                if (!_terminado) return;
                reloj.Stop();
                reloj.Dispose();
                Close();
            };
            reloj.Start();

            base.ShowDialog();
            if (_hilo != null && _hilo.Join(0)) _hilo = null;
        }

        private void IntentarConectar()
        {
            // Conexión paciente: hasta ~40 intentos con 2 s cada uno y
            // pausa de 500 ms. Cubre el primer arranque (siembra de la
            // biblia) y arranques lentos, con el launcher esperando al
            // pipe por delante. Suelo el hilo para dejar pintar la UI.
            _resultado = _ipc.ConectarPaciente(
                () =>
                {
                    _terminado = true;
                    return Cancelado;
                });
            _terminado = true;
        }

        private void Cancelar()
        {
            Cancelado = true;
            _btnCancelar.Enabled = false;
            _etiqueta.Text = "Cancelando…";
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            // Si el hilo sigue vivo por algún intento largo, no dejarlo
            // colgando el proceso: es background, así que morirá con él.
            base.OnFormClosed(e);
        }
    }
}
