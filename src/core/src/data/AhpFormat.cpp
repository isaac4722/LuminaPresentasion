// src/core/src/data/AhpFormat.cpp — Parser ahp.v1

#include "fusion/data/AhpFormat.h"

#include "json.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <atomic>

namespace fusion {

namespace {

std::atomic<std::uint32_t> g_version_leida{0};

TipoElemento ParseTipo(const std::string& s) {
    if (s == "texto")         return TipoElemento::Texto;
    if (s == "versiculo")     return TipoElemento::Versiculo;
    if (s == "imagen")        return TipoElemento::Imagen;
    if (s == "video")         return TipoElemento::Video;
    if (s == "lower_third")   return TipoElemento::LowerThird;
    if (s == "pptx")          return TipoElemento::Pptx;
    return TipoElemento::Desconocido;
}

ModoVersiculo ParseModo(const std::string& s) {
    return s == "tercio" ? ModoVersiculo::Tercio : ModoVersiculo::Completo;
}

ModoPptx ParseModoPptx(const std::string& s) {
    if (s == "com")     return ModoPptx::Com;
    if (s == "directo") return ModoPptx::Directo;
    return ModoPptx::Desconocido;
}

const char* ModoPptxATexto(ModoPptx m) {
    return m == ModoPptx::Directo ? "directo" : "com";
}

const char* AjusteATexto(AjusteImagen a) {
    switch (a) {
        case AjusteImagen::Contener: return "contener";
        case AjusteImagen::Estirar:  return "estirar";
        default:                     return "cubrir";
    }
}

AjusteImagen ParseAjuste(const std::string& s) {
    if (s == "contener")  return AjusteImagen::Contener;
    if (s == "estirar")   return AjusteImagen::Estirar;
    return AjusteImagen::Cubrir;
}

} // namespace

bool AhpFormat::Cargar(const std::string& ruta, Programa* out,
                       std::string* msg_error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { if (msg_error) *msg_error = "No se pudo abrir: " + ruta; return false; }
    std::stringstream ss; ss << f.rdbuf();
    return CargarFromString(ss.str(), out, msg_error);
}

bool AhpFormat::CargarFromString(const std::string& json_text, Programa* out,
                                  std::string* msg_error) {
    auto j = nlohmann::json::parse(json_text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (msg_error) *msg_error = "JSON inválido o no es objeto";
        return false;
    }
    if (j.value("formato", "") != "ahp") {
        if (msg_error) *msg_error = "No es un archivo ahp (falta 'formato':'ahp')";
        return false;
    }
    std::uint32_t v = j.value("version", 0);
    g_version_leida.store(v);
    if (v > kAhpVersionActual) {
        if (msg_error) *msg_error = "Versión ahp.v" + std::to_string(v)
                                  + " no soportada (máxima: v1)";
        return false;
    }

    const auto meta = j.value("meta", nlohmann::json::object());
    out->titulo    = meta.value("titulo", "");
    out->fecha     = meta.value("fecha", "");
    out->autor     = meta.value("autor", "");
    out->tema_raiz = meta.value("tema_raiz", "");
    out->notas     = meta.value("notas", "");

    out->escenarios.clear();
    const auto escenarios = j.value("escenarios", nlohmann::json::array());
    for (const auto& ej : escenarios) {
        Escenario esc;
        esc.id     = ej.value("id", "");
        esc.nombre = ej.value("nombre", "");
        esc.notas  = ej.value("notas", "");
        esc.tema   = ej.value("tema", "");
        const auto elementos = ej.value("elementos", nlohmann::json::array());
        for (const auto& elj : elementos) {
            Elemento e;
            e.id    = elj.value("id", "");
            e.tipo  = ParseTipo(elj.value("tipo", ""));
            e.titulo= elj.value("titulo", "");
            const auto lineas = elj.value("lineas", nlohmann::json::array());
            for (const auto& lj : lineas) {
                LineaTexto l;
                l.texto = lj.value("texto", "");
                l.marca = lj.value("marca", "");
                e.lineas.push_back(l);
            }
            e.acordes          = elj.value("acordes", "");
            e.tono_origen      = elj.value("tono_origen", "");
            e.tono_actual      = elj.value("tono_actual", "");
            e.bpm              = elj.value("bpm", 0);
            e.cita             = elj.value("cita", "");
            e.biblia           = elj.value("biblia", "");
            e.texto_versiculo  = elj.value("texto", "");
            // "modo" es contextual al tipo: completo/tercio para
            // versiculo, com/directo para pptx (format_ahp_v1.md).
            // Falta → valores por defecto (tolerancia con archivos
            // antiguos); valor inválido → Desconocido (Validar lo
            // rechaza con error explícito).
            const std::string modo_json = elj.value("modo", "");
            if (e.tipo == TipoElemento::Pptx) {
                e.modo_pptx = modo_json.empty() ? ModoPptx::Com
                                                : ParseModoPptx(modo_json);
            } else if (e.tipo == TipoElemento::Versiculo) {
                e.modo_versiculo = modo_json.empty()
                                       ? ModoVersiculo::Completo
                                       : ParseModo(modo_json);
            }
            e.ruta             = elj.value("ruta", "");
            e.ajuste           = ParseAjuste(elj.value("ajuste", "cubrir"));
            e.bucle_video      = elj.value("bucle", false);
            e.audio_video      = elj.value("audio", true);
            e.sub_lower        = elj.value("sub", "");
            e.tema_override    = elj.value("tema_override", "");
            e.notas            = elj.value("notas", "");
            esc.elementos.push_back(std::move(e));
        }
        out->escenarios.push_back(std::move(esc));
    }
    return true;
}

std::string AhpFormat::Serializar(const Programa& p) {
    nlohmann::json j;
    j["formato"] = "ahp";
    j["version"] = kAhpVersionActual;
    j["meta"] = {
        {"titulo", p.titulo},
        {"fecha",  p.fecha},
        {"autor",  p.autor},
        {"tema_raiz", p.tema_raiz},
        {"notas",  p.notas}
    };
    nlohmann::json escs = nlohmann::json::array();
    for (const auto& e : p.escenarios) {
        nlohmann::json ej = {
            {"id", e.id}, {"nombre", e.nombre},
            {"notas", e.notas}, {"tema", e.tema}
        };
        nlohmann::json elems = nlohmann::json::array();
        for (const auto& el : e.elementos) {
            nlohmann::json elj = {{"id", el.id}, {"titulo", el.titulo}};
            switch (el.tipo) {
                case TipoElemento::Texto:       elj["tipo"] = "texto";       break;
                case TipoElemento::Versiculo:   elj["tipo"] = "versiculo";   break;
                case TipoElemento::Imagen:      elj["tipo"] = "imagen";      break;
                case TipoElemento::Video:       elj["tipo"] = "video";       break;
                case TipoElemento::LowerThird:  elj["tipo"] = "lower_third"; break;
                case TipoElemento::Pptx:        elj["tipo"] = "pptx";        break;
                default:                         elj["tipo"] = "texto";
            }
            if (!el.lineas.empty()) {
                nlohmann::json larr = nlohmann::json::array();
                for (const auto& l : el.lineas) {
                    larr.push_back({{"texto", l.texto}, {"marca", l.marca}});
                }
                elj["lineas"] = larr;
            }
            if (!el.acordes.empty())     elj["acordes"]     = el.acordes;
            if (!el.tono_origen.empty()) elj["tono_origen"] = el.tono_origen;
            if (!el.tono_actual.empty()) elj["tono_actual"] = el.tono_actual;
            if (el.bpm)                  elj["bpm"]         = el.bpm;
            if (!el.cita.empty())        elj["cita"]       = el.cita;
            if (!el.biblia.empty())      elj["biblia"]      = el.biblia;
            if (!el.texto_versiculo.empty()) elj["texto"]   = el.texto_versiculo;
            if (el.tipo == TipoElemento::Pptx) {
                elj["modo"] = ModoPptxATexto(el.modo_pptx);
            } else if (el.tipo == TipoElemento::Versiculo) {
                elj["modo"] = el.modo_versiculo == ModoVersiculo::Tercio
                                  ? "tercio" : "completo";
            }
            if (!el.ruta.empty())       elj["ruta"]        = el.ruta;
            if (el.tipo == TipoElemento::Imagen) {
                elj["ajuste"] = AjusteATexto(el.ajuste);
            }
            if (el.tipo == TipoElemento::Video) {
                elj["bucle"] = el.bucle_video;
                elj["audio"] = el.audio_video;
            }
            if (!el.sub_lower.empty())   elj["sub"]        = el.sub_lower;
            if (!el.tema_override.empty()) elj["tema_override"] = el.tema_override;
            if (!el.notas.empty())       elj["notas"]       = el.notas;
            elems.push_back(elj);
        }
        ej["elementos"] = elems;
        escs.push_back(ej);
    }
    j["escenarios"] = escs;
    return j.dump(2);
}

bool AhpFormat::Guardar(const std::string& ruta, const Programa& p) {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    // BOM opcional (ahp.v1 lo permite)
    const char bom[] = { static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF) };
    f.write(bom, 3);
    std::string s = Serializar(p);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
    return f.good();
}

bool AhpFormat::Validar(const Programa& p, std::string* msg_error) {
    // IDs únicos de escenarios
    std::vector<std::string> ids;
    for (const auto& e : p.escenarios) ids.push_back(e.id);
    std::sort(ids.begin(), ids.end());
    for (size_t i = 1; i < ids.size(); ++i) {
        if (ids[i] == ids[i-1] && !ids[i].empty()) {
            if (msg_error) *msg_error = "Escenario con id duplicado: " + ids[i];
            return false;
        }
    }
    // IDs únicos de elementos dentro de cada escenario + tipos/modos válidos
    for (const auto& e : p.escenarios) {
        std::vector<std::string> el_ids;
        for (const auto& el : e.elementos) {
            el_ids.push_back(el.id);
            if (el.tipo == TipoElemento::Desconocido) {
                if (msg_error) {
                    *msg_error = "Elemento con tipo desconocido en " +
                                 e.nombre + ": " + el.id;
                }
                return false;
            }
            if (el.tipo == TipoElemento::Pptx &&
                el.modo_pptx == ModoPptx::Desconocido) {
                if (msg_error) {
                    *msg_error = "Elemento pptx con modo inválido en " +
                                 e.nombre + ": " + el.id +
                                 " (use \"com\" o \"directo\")";
                }
                return false;
            }
        }
        std::sort(el_ids.begin(), el_ids.end());
        for (size_t i = 1; i < el_ids.size(); ++i) {
            if (el_ids[i] == el_ids[i-1] && !el_ids[i].empty()) {
                if (msg_error) *msg_error = "Elemento con id duplicado en " + e.nombre + ": " + el_ids[i];
                return false;
            }
        }
    }
    return true;
}

std::uint32_t AhpFormat::VersionLeida() {
    return g_version_leida.load();
}

} // namespace fusion
