// src/managed/FusionHP.Managed/Forms/OperatorConsoleForm.cs — Modo operador REAL
//
// Antes: reloj sin hora, "EN PANTALLA: (nada)" fingido. Ahora: reloj
// real, estado en vivo del motor (Timer 400 ms → estado.lector), lista
// del programa real (programa.estado) y búsqueda que AGREGA al programa
// (canto.buscar / biblia.buscar). Doble clic = proyectar.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class OperatorConsoleForm : Form
    {
        private readonly IpcClient _ipc;
        private System.Windows.Forms.Timer _timer;

        // Acción adjunta a cada renglón de lstPrograma.
        private sealed class Accion
        {
            public string Tipo;      // proyectar | canto | versiculo
            public string Escenario;
            public string Elemento;
            public int CantoId;
            public string Cita;
            public string Texto;
            public override string ToString() { return Texto; }
        }

        public OperatorConsoleForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            txtBuscarPrograma.KeyDown += txtBuscarPrograma_KeyDown;
            lstPrograma.DoubleClick += lstPrograma_DoubleClick;

            CargarPrograma();
            _timer = new System.Windows.Forms.Timer { Interval = 400 };
            _timer.Tick += (s, ev) => Refrescar();
            _timer.Start();
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            if (_timer != null) { _timer.Stop(); _timer.Dispose(); _timer = null; }
            base.OnFormClosed(e);
        }

        // --- Estado en vivo -------------------------------------------------------
        private void Refrescar()
        {
            var e = _ipc.Pedir("estado.lector", "{}");
            if (e == null) return;

            lblReloj.Text = DateTime.Now.ToString("HH:mm:ss");
            string el = IpcJson.Texto(e, "elemento_titulo");
            string esc = IpcJson.Texto(e, "escenario_nombre");
            int linea = IpcJson.Entero(e, "linea_actual");
            bool visible = IpcJson.Booleano(e, "salida_visible");
            bool negro = IpcJson.Booleano(e, "negro");
            bool logo = IpcJson.Booleano(e, "logo");
            string monitor = IpcJson.Texto(e, "monitor_dispositivo", "(auto)");

            lblEnPantalla.Text = "EN PANTALLA: " +
                (el.Length > 0 ? el : esc.Length > 0 ? esc : "(nada)") +
                "  ·  línea " + (linea + 1);
            lblSiguiente.Text = "Salida: " + (visible ? "VISIBLE" : "oculta") +
                (negro ? "  ·  NEGRO" : "") + (logo ? "  ·  LOGO" : "") +
                "  ·  " + monitor;
        }

        // --- Programa y búsqueda ---------------------------------------------------
        private void CargarPrograma()
        {
            lstPrograma.Items.Clear();

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
                        string escId = IpcJson.Texto(esc, "id");
                        string escNombre = IpcJson.Texto(esc, "nombre");
                        lstPrograma.Items.Add(new Accion
                        {
                            Tipo = "proyectar",
                            Escenario = escId,
                            Texto = "▶ " + escNombre
                        });
                        var elementos = IpcJson.Arreglo(esc, "elementos");
                        if (elementos == null) continue;
                        foreach (var itemEl in elementos)
                        {
                            var el = IpcJson.ObjetoEn(itemEl);
                            if (el == null) continue;
                            lstPrograma.Items.Add(new Accion
                            {
                                Tipo = "proyectar",
                                Escenario = escId,
                                Elemento = IpcJson.Texto(el, "id"),
                                Texto = "     " + IpcJson.Texto(el, "titulo")
                            });
                        }
                    }
                }
            }
            lstPrograma.Items.Add(new Accion
            {
                Tipo = "texto",
                Texto = "— Escribe arriba: buscar cantos y versículos —"
            });
        }

        private void txtBuscarPrograma_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode != Keys.Enter) return;
            e.Handled = true;
            string q = txtBuscarPrograma.Text.Trim();
            if (q.Length == 0) { CargarPrograma(); return; }

            lstPrograma.Items.Clear();
            bool hay = false;

            var rc = _ipc.Pedir("canto.buscar",
                "{\"texto\":\"" + Escapar(q) + "\",\"limite\":20}");
            if (rc != null && IpcJson.Ok(rc))
            {
                var arr = IpcJson.Arreglo(rc, "resultados");
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var c = IpcJson.ObjetoEn(item);
                        if (c == null) continue;
                        hay = true;
                        lstPrograma.Items.Add(new Accion
                        {
                            Tipo = "canto",
                            CantoId = IpcJson.Entero(c, "id"),
                            Texto = "♪ canto: " + IpcJson.Texto(c, "titulo")
                        });
                    }
                }
            }

            var rv = _ipc.Pedir("biblia.buscar",
                "{\"texto\":\"" + Escapar(q) + "\",\"limite\":20}");
            if (rv != null && IpcJson.Ok(rv))
            {
                var arr = IpcJson.Arreglo(rv, "resultados");
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var v = IpcJson.ObjetoEn(item);
                        if (v == null) continue;
                        hay = true;
                        lstPrograma.Items.Add(new Accion
                        {
                            Tipo = "versiculo",
                            Cita = IpcJson.Texto(v, "libro") + " " +
                                   IpcJson.Entero(v, "capitulo") + ":" +
                                   IpcJson.Entero(v, "versiculo"),
                            Texto = "✝ " + IpcJson.Texto(v, "libro") + " " +
                                    IpcJson.Entero(v, "capitulo") + ":" +
                                    IpcJson.Entero(v, "versiculo") + "  —  " +
                                    Recortar(IpcJson.Texto(v, "texto"), 60)
                        });
                    }
                }
            }
            if (!hay)
                lstPrograma.Items.Add(new Accion
                { Tipo = "texto", Texto = "(sin resultados para \"" + q + "\")" });
        }

        private void lstPrograma_DoubleClick(object sender, EventArgs e)
        {
            var a = lstPrograma.SelectedItem as Accion;
            if (a == null) return;

            if (a.Tipo == "proyectar")
            {
                string payload = "{\"escenario_id\":\"" + a.Escenario + "\"";
                if (a.Elemento != null)
                    payload += ",\"elemento_id\":\"" + a.Elemento + "\"";
                _ipc.Enviar(a.Elemento != null
                                ? "proyeccion.elemento"
                                : "proyeccion.escenario",
                            payload + "}");
                _ipc.Enviar("proyeccion.iniciar", "{}");
            }
            else if (a.Tipo == "canto")
            {
                var r = _ipc.Pedir("programa.agregar_canto",
                                   "{\"id\":" + a.CantoId + "}");
                if (r != null && IpcJson.Ok(r)) ProyectarAgregado(r);
            }
            else if (a.Tipo == "versiculo")
            {
                var r = _ipc.Pedir("programa.agregar_versiculo",
                                   "{\"cita\":\"" + Escapar(a.Cita) + "\"}");
                if (r != null && IpcJson.Ok(r)) ProyectarAgregado(r);
            }
        }

        private void ProyectarAgregado(Dictionary<string, object> r)
        {
            string esc = IpcJson.Texto(r, "escenario_id");
            string el = IpcJson.Texto(r, "elemento_id");
            _ipc.Enviar("proyeccion.elemento",
                        "{\"escenario_id\":\"" + esc +
                        "\",\"elemento_id\":\"" + el + "\"}");
            _ipc.Enviar("proyeccion.iniciar", "{}");
        }

        // --- Transporte --------------------------------------------------------------
        private void btnSiguiente_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.siguiente", "{}");
        }

        private void btnAnterior_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.anterior", "{}");
        }

        private void btnNegro_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.negro", "{\"activar\":true}");
        }

        private void btnLogo_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.logo", "{\"activar\":true}");
        }

        private void btnOcultar_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.detener", "{}");
        }

        private static string Escapar(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static string Recortar(string s, int n)
        {
            if (string.IsNullOrEmpty(s)) return "";
            return s.Length <= n ? s : s.Substring(0, n) + "…";
        }
    }
}
