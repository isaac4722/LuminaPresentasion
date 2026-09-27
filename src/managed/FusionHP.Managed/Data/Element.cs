// src/managed/FusionHP.Managed/Data/Element.cs

using System.Collections.Generic;

namespace FusionHP.Managed.Data
{
    public enum TipoElemento { Texto, Versiculo, Imagen, Video, LowerThird, Pptx, Desconocido }
    public enum ModoVersiculo { Completo, Tercio }
    public enum AjusteImagen { Cubrir, Contener, Estirar }

    public class LineaTexto
    {
        public string Texto = "";
        public string Marca = "";
    }

    public class Element
    {
        public string Id = "";
        public TipoElemento Tipo = TipoElemento.Texto;
        public string Titulo = "";
        public List<LineaTexto> Lineas = new List<LineaTexto>();
        public string Acordes = "";
        public string TonoOrigen = "";
        public string TonoActual = "";
        public int Bpm = 0;
        public string Cita = "";          // versiculo
        public string Biblia = "";
        public string TextoVersiculo = "";
        public ModoVersiculo Modo = ModoVersiculo.Completo;
        public string Ruta = "";           // imagen/video/pptx
        public AjusteImagen Ajuste = AjusteImagen.Cubrir;
        public bool BucleVideo = false;
        public bool AudioVideo = true;
        public string SubLower = "";
        public string TemaOverride = "";
        public string Notas = "";
    }
}
