// src/core/src/core/VideoPlayer.cpp — DirectShow VMR-9 (stub fundacional)

#include "fusion/core/VideoPlayer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dshow.h>
#include <vmr9.h>

#include <string>

namespace fusion {

struct VideoPlayer::Impl {
    std::wstring ruta;
    bool bucle = false;
    bool audio = true;
    RECT rect = {0,0,0,0};
    // TODO(P0): ComPtr<IGraphBuilder>, IMediaControl, IMediaEventEx, IVMR9.
};

VideoPlayer::VideoPlayer()  : impl_(std::make_unique<Impl>()) {}
VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::Cargar(const std::string& ruta, void* hwnd_padre) {
    // TODO(P0): construir grafo DirectShow con VMR-9 sin ventanas.
    impl_->ruta = std::wstring(ruta.begin(), ruta.end());
    (void)hwnd_padre;
    return true;
}

void VideoPlayer::Descargar() {
    impl_->ruta.clear();
}

bool VideoPlayer::Reproducir() { return true; }
bool VideoPlayer::Pausar()     { return true; }
bool VideoPlayer::Detener()    { return true; }

void VideoPlayer::SetBucle(bool b) { impl_->bucle = b; }
void VideoPlayer::SetAudio(bool a) { impl_->audio = a; }

void VideoPlayer::AjustarA(int x, int y, int w, int h) {
    impl_->rect = {x, y, x + w, y + h};
}

} // namespace fusion
