// src/managed/FusionHP.Managed/Forms/LibraryForm.cs — Biblioteca REAL
//
// Antes: 3 cantos inventados, "Libro 1..Libro 66" y una lista de biblias
// fingida. Ahora TODO viene del núcleo por ipc.v1:
//   - canto.listar (con id) + filtro en vivo + doble clic = agregar al
//     programa del motor (que lo construye de verdad, sección por sección).
//   - biblia.listar + biblia.seleccionar (la activa cambia de verdad).
//   - biblia.libros → árbol real de 66 libros → biblia.capitulos →
//     biblia.capitulo con el texto completo en el panel derecho.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class LibraryForm : Form
    {
        private readonly IpcClient _ipc;

        private List<Dictionary<string, object>> _cantos =
            new List<Dictionary<string, object>>();
        private bool _cargandoBiblias = false;
        private string _citaSeleccionada = "";

        public LibraryForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarCantos();
            CargarBiblias();
            CargarArbolBiblia();
        }

        // --- Cantos ---------------------------------------------------------
        private void CargarCantos()
        {
            _cantos.Clear();
            lstCantos.Items.Clear();

            var r = _ipc.Pedir("canto.listar", "{}");
            if (r == null)
            {
                lstCantos.Items.Add("(el núcleo no respondió)");
                return;
            }
            if (!IpcJson.Ok(r))
            {
                lstCantos.Items.Add("(error: " + IpcJson.MensajeError(r) + ")");
                return;
            }

            var arr = IpcJson.Arreglo(r, "cantos");
            if (arr != null)
            {
                foreach (var item in arr)
                {
                    var c = IpcJson.ObjetoEn(item);
                    if (c == null) continue;
                    _cantos.Add(c);
                }
            }
            if (_cantos.Count == 0)
            {
                lstCantos.Items.Add("(cancionero vacío: importa cantos)");
                return;
            }
            LlenarCantos("");
        }

        private void LlenarCantos(string filtro)
        {
            lstCantos.Items.Clear();
            filtro = (filtro ?? "").Trim().ToLowerInvariant();
            foreach (var c in _cantos)
            {
                string titulo = IpcJson.Texto(c, "titulo");
                string autor = IpcJson.Texto(c, "autor");
                if (filtro.Length > 0 &&
                    !(titulo.ToLowerInvariant().Contains(filtro) ||
                      autor.ToLowerInvariant().Contains(filtro)))
                    continue;
                string tono = IpcJson.Texto(c, "tono_origen", "");
                lstCantos.Items.Add(new ElementoLista(
                    titulo + (autor.Length > 0 ? "  —  " + autor : "") +
                    (tono.Length > 0 ? "  [" + tono + "]" : ""),
                    c));
            }
            if (lstCantos.Items.Count == 0)
                lstCantos.Items.Add("(sin coincidencias)");
        }

        private void txtBuscarCanto_TextChanged(object sender, EventArgs e)
        {
            LlenarCantos(txtBuscarCanto.Text);
        }

        private void lstCantos_DoubleClick(object sender, EventArgs e)
        {
            var sel = lstCantos.SelectedItem as ElementoLista;
            if (sel == null || !(sel.Datos is Dictionary<string, object>))
                return;
            var canto = (Dictionary<string, object>)sel.Datos;
            int id = IpcJson.Entero(canto, "id");
            string titulo = IpcJson.Texto(canto, "titulo");

            var r = _ipc.Pedir("programa.agregar_canto",
                               "{\"id\":" + id + "}");
            if (r == null || !IpcJson.Ok(r))
            {
                MessageBox.Show(this, "No se pudo agregar el canto:\n" +
                                IpcJson.MensajeError(r),
                                "FUSION-HP", MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
                return;
            }
            string esc = IpcJson.Texto(r, "escenario_id");
            lblBibliaTitulo.Text = "Biblia   ·   agregado al programa: " +
                                   titulo + " (" + esc + ")";
        }

        // --- Biblias ----------------------------------------------------------
        private void CargarBiblias()
        {
            _cargandoBiblias = true;
            cmbBiblias.Items.Clear();

            var r = _ipc.Pedir("biblia.listar", "{}");
            if (r != null && IpcJson.Ok(r))
            {
                string activa = IpcJson.Texto(r, "activa");
                var arr = IpcJson.Arreglo(r, "biblias");
                int idx = -1, activaIdx = 0;
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        string nombre = Convert.ToString(item);
                        if (nombre == activa) activaIdx = idx + 1;
                        cmbBiblias.Items.Add(new ElementoLista(nombre, nombre));
                        ++idx;
                    }
                }
                if (cmbBiblias.Items.Count > 0)
                    cmbBiblias.SelectedIndex = activaIdx;
            }
            else
            {
                cmbBiblias.Items.Add(new ElementoLista("(sin biblias)", ""));
                cmbBiblias.SelectedIndex = 0;
            }
            _cargandoBiblias = false;
        }

        private void cmbBiblias_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_cargandoBiblias) return;
            var sel = cmbBiblias.SelectedItem as ElementoLista;
            if (sel == null || string.IsNullOrEmpty((string)sel.Datos)) return;
            string nombre = (string)sel.Datos;
            string payload = "{\"nombre\":\"" + Nombre(nombre) + "\"}";
            var r = _ipc.Pedir("biblia.seleccionar", payload);
            if (r == null || !IpcJson.Ok(r))
            {
                MessageBox.Show(this, "No se pudo cambiar la biblia:\n" +
                                IpcJson.MensajeError(r),
                                "FUSION-HP", MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
                return;
            }
            CargarArbolBiblia();   // re-leer el árbol con la biblia activa
        }

        // --- Árbol de la biblia -------------------------------------------------
        private void CargarArbolBiblia()
        {
            treeBiblia.Nodes.Clear();
            treeBiblia.BeginUpdate();

            var raizAt = treeBiblia.Nodes.Add("Antiguo Testamento");
            var raizNt = treeBiblia.Nodes.Add("Nuevo Testamento");

            var r = _ipc.Pedir("biblia.libros", "{}");
            if (r == null || !IpcJson.Ok(r))
            {
                raizAt.Nodes.Add("(el núcleo no devolvió libros)");
                treeBiblia.EndUpdate();
                return;
            }

            var libros = IpcJson.Arreglo(r, "libros");
            if (libros != null)
            {
                foreach (var item in libros)
                {
                    var lb = IpcJson.ObjetoEn(item);
                    if (lb == null) continue;
                    string nombre = IpcJson.Texto(lb, "nombre");
                    string test = IpcJson.Texto(lb, "testamento");
                    int numero = IpcJson.Entero(lb, "numero");
                    TreeNode padre = test == "NT" ? raizNt : raizAt;
                    var nodo = padre.Nodes.Add(new TreeNode(nombre)
                    {
                        Tag = new InfoNodoLibro { Numero = numero, EsLibro = true }
                    });
                    nodo.Nodes.Add(new TreeNode("..."));   // capítulos perezosos
                }
            }
            treeBiblia.EndUpdate();
            if (raizAt.Nodes.Count > 0) raizAt.Expand();
            if (raizNt.Nodes.Count > 0) raizNt.Expand();
        }

        private void treeBiblia_AfterSelect(object sender, TreeViewEventArgs e)
        {
            var nodo = e.Node;
            if (nodo == null) return;

            var info = nodo.Tag as InfoNodoLibro;
            if (info == null) return;

            if (info.EsLibro && nodo.Nodes.Count == 1 &&
                nodo.Nodes[0].Text == "...")
            {
                // Capítulos perezosos: biblia.capitulos {libro}.
                nodo.Nodes.Clear();
                string payload = "{\"libro\":" + info.Numero + "}";
                var r = _ipc.Pedir("biblia.capitulos", payload);
                if (r != null && IpcJson.Ok(r))
                {
                    var caps = IpcJson.Arreglo(r, "capitulos");
                    if (caps != null)
                    {
                        foreach (var item in caps)
                        {
                            int cap = Convert.ToInt32(item);
                            nodo.Nodes.Add(new TreeNode("Capítulo " + cap)
                            {
                                Tag = new InfoNodoLibro
                                {
                                    Numero = info.Numero,
                                    Capitulo = cap,
                                    NombreLibro = nodo.Text,
                                    EsLibro = false
                                }
                            });
                        }
                    }
                    nodo.Expand();
                }
                return;
            }

            if (!info.EsLibro)
            {
                // Capítulo elegido: biblia.capitulo → texto completo.
                var r = _ipc.Pedir("biblia.capitulo",
                    "{\"libro\":" + info.Numero +
                    ",\"capitulo\":" + info.Capitulo + "}");
                var sb = new StringBuilder();
                sb.Append(info.NombreLibro).Append(' ')
                  .Append(info.Capitulo).AppendLine();
                sb.AppendLine();
                if (r != null && IpcJson.Ok(r))
                {
                    var vers = IpcJson.Arreglo(r, "versiculos");
                    bool hay = false;
                    if (vers != null)
                    {
                        foreach (var item in vers)
                        {
                            var v = IpcJson.ObjetoEn(item);
                            if (v == null) continue;
                            sb.Append(IpcJson.Entero(v, "versiculo"))
                              .Append("  ")
                              .Append(IpcJson.Texto(v, "texto"))
                              .AppendLine();
                            hay = true;
                        }
                    }
                    if (!hay) sb.Append("(capítulo sin versículos)");
                }
                else
                {
                    sb.Append("(error: ").Append(IpcJson.MensajeError(r))
                      .Append(')');
                }
                txtVersiculo.Text = sb.ToString();
                _citaSeleccionada = info.NombreLibro + " " + info.Capitulo;
            }
        }

        private void btnAgregarVersiculo_Click(object sender, EventArgs e)
        {
            if (_citaSeleccionada.Length == 0)
            {
                MessageBox.Show(this, "Elige primero un capítulo del árbol.",
                                "FUSION-HP", MessageBoxButtons.OK,
                                MessageBoxIcon.Information);
                return;
            }
            string payload = "{\"cita\":\"" + Nombre(_citaSeleccionada) +
                             "\"}";
            var r = _ipc.Pedir("programa.agregar_versiculo", payload);
            if (r != null && IpcJson.Ok(r))
            {
                string esc = IpcJson.Texto(r, "escenario_id");
                lblBibliaTitulo.Text = "Biblia   ·   versículo agregado (" +
                                       esc + ")";
            }
            else
            {
                MessageBox.Show(this, "No se pudo agregar el versículo:\n" +
                                IpcJson.MensajeError(r),
                                "FUSION-HP", MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
        }

        private static string Nombre(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        // Etiquetas con datos adjuntos (para listas y árbol).
        private sealed class ElementoLista
        {
            public readonly string Texto;
            public readonly object Datos;
            public ElementoLista(string texto, object datos)
            {
                Texto = texto; Datos = datos;
            }
            public override string ToString() { return Texto; }
        }

        private sealed class InfoNodoLibro
        {
            public int Numero;
            public int Capitulo;
            public string NombreLibro = "";
            public bool EsLibro;
        }
    }
}
