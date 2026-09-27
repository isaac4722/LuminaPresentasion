// src/core/include/fusion/core/VideoPlayer.h
// Reproductor de vídeo con DirectShow (VMR-9 sin ventanas).

#pragma once

#include <memory>
#include <string>

namespace fusion {

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer&)            = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    bool Cargar(const std::string& ruta, void* hwnd_padre);
    void Descargar();

    bool Reproducir();
    bool Pausar();
    bool Detener();

    void SetBucle(bool bucle);
    void SetAudio(bool audio);

    // Para incrustar el vídeo en la salida de proyección.
    void AjustarA(int x, int y, int w, int h);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
