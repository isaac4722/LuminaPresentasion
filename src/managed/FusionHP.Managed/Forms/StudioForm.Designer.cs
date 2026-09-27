namespace FusionHP.Managed.Forms
{
    partial class StudioForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.MenuStrip menu;
        private System.Windows.Forms.ToolStripMenuItem archivoToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem nuevoToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem abrirToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem guardarToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exportarCSVToolStripMenuItem;
        private System.Windows.Forms.ListBox lstEscenarios;
        private System.Windows.Forms.Button btnAgregarEscenario;
        private System.Windows.Forms.Panel lienzo;
        private System.Windows.Forms.TextBox txtNotas;

        private void InitializeComponent()
        {
            menu = new System.Windows.Forms.MenuStrip();
            archivoToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            nuevoToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            abrirToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            guardarToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            exportarCSVToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            lstEscenarios = new System.Windows.Forms.ListBox();
            btnAgregarEscenario = new System.Windows.Forms.Button();
            lienzo = new System.Windows.Forms.Panel();
            txtNotas = new System.Windows.Forms.TextBox();

            menu.Items.Add(archivoToolStripMenuItem);
            archivoToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripMenuItem[] {
                nuevoToolStripMenuItem, abrirToolStripMenuItem, guardarToolStripMenuItem, exportarCSVToolStripMenuItem
            });
            archivoToolStripMenuItem.Text = "Archivo";
            nuevoToolStripMenuItem.Text = "Nuevo";
            nuevoToolStripMenuItem.Click += new System.EventHandler(nuevoToolStripMenuItem_Click);
            abrirToolStripMenuItem.Text = "Abrir...";
            abrirToolStripMenuItem.Click += new System.EventHandler(abrirToolStripMenuItem_Click);
            guardarToolStripMenuItem.Text = "Guardar...";
            guardarToolStripMenuItem.Click += new System.EventHandler(guardarToolStripMenuItem_Click);
            exportarCSVToolStripMenuItem.Text = "Exportar historial a CSV";
            exportarCSVToolStripMenuItem.Click += new System.EventHandler(exportarCSVToolStripMenuItem_Click);

            lstEscenarios.Location = new System.Drawing.Point(0, 27);
            lstEscenarios.Size = new System.Drawing.Size(220, 500);

            btnAgregarEscenario.Location = new System.Drawing.Point(0, 530);
            btnAgregarEscenario.Size = new System.Drawing.Size(220, 30);
            btnAgregarEscenario.Text = "+ Agregar escenario";
            btnAgregarEscenario.Click += new System.EventHandler(btnAgregarEscenario_Click);

            lienzo.BackColor = System.Drawing.Color.Black;
            lienzo.Location = new System.Drawing.Point(220, 27);
            lienzo.Size = new System.Drawing.Size(960, 540);

            txtNotas.Location = new System.Drawing.Point(220, 580);
            txtNotas.Size = new System.Drawing.Size(960, 60);
            txtNotas.Multiline = true;
            txtNotas.Text = "Notas del escenario...";

            ClientSize = new System.Drawing.Size(1200, 700);
            Controls.Add(menu);
            Controls.Add(lstEscenarios);
            Controls.Add(btnAgregarEscenario);
            Controls.Add(lienzo);
            Controls.Add(txtNotas);
            MainMenuStrip = menu;
            Text = "FUSION-HP — Estudio";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
