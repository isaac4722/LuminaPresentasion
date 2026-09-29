// src/managed/FusionHP.Managed/Forms/PresentForm.cs — Presentar REAL
//
// Antes: monitores inventados y botones que gritaban a un núcleo que
// nunca dibujaba. Ahora: monitores reales (monitor.listar), el programa
// real del motor (programa.estado) navegable con clic, y estado en vivo
// (Timer sobre estado.lector). La proyección dibuja de verdad (núcleo).

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class PresentForm : Form
    {
        private readonly IpcClient _ipc;
        private System.Windows.Forms.Timer _timer;
        private TreeView _arbolPrograma;

        public PresentForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            ConstruirArbolPrograma();
            CargarMonitores();
            CargarPrograma();

            _timer = new System.Windows.Forms.Timer { Interval = 500 };
            _timer.Tick += (s, ev) => RefrescarEstado();
            _timer.Start();
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            if (_timer != null) { _timer.Stop(); _timer.Dispose(); _timer = null; }
            base.OnFormClosed(e);
        }

        // --- Monitores ----------------------------------------------------------
        private void CargarMonitores()
        {
            cmbMonitores.Items.Clear();
            var r = _ipc.Pedir("monitor.listar", "{}");
            if (r != null && IpcJson.Ok(r))
            {
                var arr = IpcJson.Arreglo(r, "monitores");
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var m = IpcJson.ObjetoEn(item);
                        if (m == null) continue;
                        string disp = IpcJson.Texto(m, "dispositivo");
                        string nombre = IpcJson.Texto(m, "nombre", disp);
                        bool primario = IpcJson.Booleano(m, "primario");
                        cmbMonitores.Items.Add(new ElementoMonitor(
                            nombre + (primario ? " (primario)" : ""), disp));
                    }
                }
            }
            if (cmbMonitores.Items.Count > 0) cmbMonitores.SelectedIndex = 0;
        }

        private void cmbMonitores_SelectedIndexChanged(object sender, EventArgs e)
        {
            var sel = cmbMonitores.SelectedItem as ElementoMonitor;
            if (sel == null) return;
            string payload = "{\"dispositivo\":\"" +
                sel.Dispositivo.Replace("\\", "\\\\") + "\"}";
            _ipc.Pedir("monitor.seleccionar", payload);
        }

        // --- Programa navegable ---------------------------------------------------
        private void ConstruirArbolPrograma()
        {
            _arbolPrograma = new TreeView
            {
                Dock = DockStyle.Fill,
                HideSelection = false,
                FullRowSelect = true
            };
            _arbolPrograma.AfterSelect += ArbolPrograma_AfterSelect;
            pnlMiniaturas.Controls.Add(_arbolPrograma);
        }

        private void CargarPrograma()
        {
            _arbolPrograma.BeginUpdate();
            _arbolPrograma.Nodes.Clear();

            var r = _ipc.Pedir("programa.estado", "{}");
            var prog = (r != null && IpcJson.Ok(r))
                ? IpcJson.Objeto(r, "programa") : null;

            if (prog == null)
            {
                _arbolPrograma.Nodes.Add(
                    "(sin programa: agrega cantos o versículos desde la Biblioteca)");
                _arbolPrograma.EndUpdate();
                return;
            }

            string titulo = "";
            var meta = IpcJson.Objeto(prog, "meta");
            if (meta != null) titulo = IpcJson.Texto(meta, "titulo");
            _arbolPrograma.Nodes.Add(new TreeNode(
                "Programa: " + (titulo.Length > 0 ? titulo : "(sin título)"))
            { ForeColor = System.Drawing.Color.DimGray });

            var escenarios = IpcJson.Arreglo(prog, "escenarios");
            if (escenarios != null)
            {
                foreach (var itemEsc in escenarios)
                {
                    var esc = IpcJson.ObjetoEn(itemEsc);
                    if (esc == null) continue;
                    string escId = IpcJson.Texto(esc, "id");
                    string escNombre = IpcJson.Texto(esc, "nombre");
                    var nodoEsc = new TreeNode(escNombre)
                    { Tag = new RefProyeccion { Escenario = escId } };
                    var elementos = IpcJson.Arreglo(esc, "elementos");
                    if (elementos != null)
                    {
                        foreach (var itemEl in elementos)
                        {
                            var el = IpcJson.ObjetoEn(itemEl);
                            if (el == null) continue;
                            string elId = IpcJson.Texto(el, "id");
                            string elTitulo = IpcJson.Texto(el, "titulo");
                            string tipo = IpcJson.Texto(el, "tipo", "texto");
                            nodoEsc.Nodes.Add(new TreeNode(
                                elTitulo + "   [" + tipo + "]")
                            {
                                Tag = new RefProyeccion
                                {
                                    Escenario = escId,
                                    Elemento = elId
                                }
                            });
                        }
                    }
                    _arbolPrograma.Nodes.Add(nodoEsc);
                }
            }
            _arbolPrograma.ExpandAll();
            _arbolPrograma.EndUpdate();
        }

        private void ArbolPrograma_AfterSelect(object sender, TreeViewEventArgs e)
        {
            var ref_ = e.Node?.Tag as RefProyeccion;
            if (ref_ == null) return;

            string payload = "{\"escenario_id\":\"" + ref_.Escenario + "\"";
            if (ref_.Elemento != null)
                payload += ",\"elemento_id\":\"" + ref_.Elemento + "\"";
            payload += "}";
            _ipc.Enviar(ref_.Elemento != null
                            ? "proyeccion.elemento" : "proyeccion.escenario",
                        payload);
        }

        // --- Estado en vivo ---------------------------------------------------------
        private void RefrescarEstado()
        {
            var e = _ipc.Pedir("estado.lector", "{}");
            if (e == null) return;

            string programa = IpcJson.Texto(e, "programa_titulo");
            string esc = IpcJson.Texto(e, "escenario_nombre");
            string el = IpcJson.Texto(e, "elemento_titulo");
            int linea = IpcJson.Entero(e, "linea_actual");
            bool visible = IpcJson.Booleano(e, "salida_visible");
            bool negro = IpcJson.Booleano(e, "negro");
            bool logo = IpcJson.Booleano(e, "logo");

            lblStatus.Text =
                (programa.Length > 0 ? programa : "sin programa") +
                "  ·  " + (el.Length > 0 ? el : esc.Length > 0 ? esc : "—") +
                "  ·  línea " + (linea + 1) +
                "  ·  salida: " + (visible ? "VISIBLE" : "oculta") +
                (negro ? "  ·  NEGRO" : "") +
                (logo ? "  ·  LOGO" : "");
        }

        // --- Botones y atajos (Holyrics) -------------------------------------------
        private void btnIniciar_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.iniciar", "{}");
            CargarPrograma();
        }

        private void btnDetener_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.detener", "{}");
        }

        private void btnNegro_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.negro", "{\"activar\":true}");
        }

        private void btnLogo_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.logo", "{\"activar\":true}");
        }

        // Atajos Holyrics: flechas, Espacio, Esc, B/C/L, G, F5, F8
        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            switch (keyData)
            {
                case Keys.Escape: _ipc.Enviar("proyeccion.detener", "{}"); Close(); return true;
                case Keys.Space:
                case Keys.Right:
                case Keys.Down:    _ipc.Enviar("proyeccion.siguiente", "{}"); return true;
                case Keys.Left:
                case Keys.Up:      _ipc.Enviar("proyeccion.anterior", "{}"); return true;
                case Keys.B:       _ipc.Enviar("proyeccion.negro", "{\"activar\":true}"); return true;
                case Keys.C:       _ipc.Enviar("proyeccion.logo",  "{\"activar\":false}"); return true;
                case Keys.L:       _ipc.Enviar("proyeccion.logo",  "{\"activar\":true}"); return true;
                case Keys.G:       using (var f = new BibleQuickForm(_ipc)) f.ShowDialog(this); return true;
                case Keys.F5:      _ipc.Enviar("proyeccion.iniciar", "{}"); CargarPrograma(); return true;
                case Keys.F8:      using (var f = new OperatorConsoleForm(_ipc)) f.ShowDialog(this); return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        private sealed class ElementoMonitor
        {
            public readonly string Texto;
            public readonly string Dispositivo;
            public ElementoMonitor(string texto, string disp)
            {
                Texto = texto; Dispositivo = disp;
            }
            public override string ToString() { return Texto; }
        }

        private sealed class RefProyeccion
        {
            public string Escenario = "";
            public string Elemento;   // null = nodo de escenario
        }
    }
}
