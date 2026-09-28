// src/launcher/main.cpp — Launcher nativo de FUSION-HP
//
// Punto único de arranque. Sin consola (WinMain). Verifica .NET 4.8,
// instala si falta el instalador offline oficial incluido (una sola vez,
// con aviso), arranca el núcleo si no está corriendo y abre la consola
// gestionada. Si la consola gestionada no está disponible (falta .NET o
// archivos), abre el shell Qt como alternativa de escritorio.
//
// Ciclo de vida: el launcher permanece vivo (sin ventana) hasta que sus
// hijos terminan. El Job Object garantiza que NO queden procesos colgados:
// si el launcher muere, el núcleo y la consola mueren con él; cuando la
// consola gestionada se cierra, el launcher corta el núcleo y sale.
//
// Diagnóstico: cada paso se registra en runtime\arranque.log (en la raíz
// de datos ESCRIBIBLE: junto al exe si es portable; %LOCALAPPDATA%\FUSION-HP
// si está instalado en Program Files) y, si no pudo abrirse NINGUNA
// interfaz, se muestra un aviso con la causa (nunca un cierre silencioso).
// Además, el arranque VERIFICA que FusionCore.exe sigue vivo y que el
// pipe IPC apareció: "arrancado" ya no se registra sin haber comprobado.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>   // ShellExecuteExW (instalador offline de .NET)

#include <string>
#include <vector>

namespace {

// Clave de Registro (solo LECTURA) donde Windows reporta la versión de .NET 4.x.
constexpr const wchar_t* kNet48Key   = L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full";
constexpr const wchar_t* kNet48Value = L"Release";
// 528040 = .NET 4.8 RTM. Cualquier valor >= este es 4.8.
constexpr DWORD kNet48MinRelease = 528040;

// ---------------------------------------------------------------------------
// Helpers de rutas (junto al ejecutable del launcher)
// ---------------------------------------------------------------------------
std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? p.substr(0, pos) : L".";
}

// Raíz de datos escribibles, MISMA política que el núcleo (Rutas.cpp):
// carpeta del exe si se puede escribir; si no, %LOCALAPPDATA%\FUSION-HP
// (instalado en Program Files: sin esto ni siquiera el arranque.log se
// podía escribir).
std::wstring RaizDatos() {
    static std::wstring raiz = [] {
        std::wstring exe = ExeDir();
        std::wstring prueba = exe + L"\\fusion_escritura.prueba";
        HANDLE h = CreateFileW(prueba.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            DeleteFileW(prueba.c_str());
            return exe;   // portable de verdad
        }
        wchar_t buf[MAX_PATH] = {0};
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::wstring dir = std::wstring(buf) + L"\\FUSION-HP";
            CreateDirectoryW(dir.c_str(), nullptr);
            CreateDirectoryW((dir + L"\\runtime").c_str(), nullptr);
            return dir;
        }
        return exe;   // último recurso
    }();
    return raiz;
}

std::wstring RuntimeDir() { return RaizDatos() + L"\\runtime"; }

bool ArchivoExiste(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---------------------------------------------------------------------------
// Bitácora de arranque (runtime\arranque.log). Sin Registro, solo disco.
// ---------------------------------------------------------------------------
void RegistrarLinea(const std::wstring& linea) {
    CreateDirectoryW(RuntimeDir().c_str(), nullptr);
    HANDLE h = CreateFileW((RuntimeDir() + L"\\arranque.log").c_str(),
                           FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t sello[64];
    swprintf_s(sello, 64, L"[%04u-%02u-%02u %02u:%02u:%02u] ",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    DWORD escritos = 0;
    WriteFile(h, sello, static_cast<DWORD>(wcslen(sello) * sizeof(wchar_t)),
              &escritos, nullptr);
    WriteFile(h, linea.c_str(),
              static_cast<DWORD>(linea.size() * sizeof(wchar_t)),
              &escritos, nullptr);
    WriteFile(h, L"\r\n", 2 * sizeof(wchar_t), &escritos, nullptr);
    CloseHandle(h);
}

std::wstring MensajeError(DWORD rc) {
    LPWSTR buf = nullptr;
    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, rc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring out = (n && buf) ? std::wstring(buf, n) : L"";
    if (buf) LocalFree(buf);
    while (!out.empty() &&
           (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' '))
        out.pop_back();
    return out.empty() ? (L"código " + std::to_wstring(rc)) : out;
}

// ---------------------------------------------------------------------------
// Comprueba si .NET 4.8 está instalado (lectura del Registro, sin escribir).
// ---------------------------------------------------------------------------
bool Net48Instalado() {
    HKEY h;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kNet48Key, 0, KEY_READ, &h) != ERROR_SUCCESS) {
        return false;
    }
    DWORD release = 0;
    DWORD sz = sizeof(release);
    LSTATUS rc = RegQueryValueExW(h, kNet48Value, nullptr, nullptr,
                                   reinterpret_cast<LPBYTE>(&release), &sz);
    RegCloseKey(h);
    if (rc != ERROR_SUCCESS) return false;
    return release >= kNet48MinRelease;
}

// ---------------------------------------------------------------------------
// Instalador offline de .NET 4.8 (installer/net48_offline; el launcher lo
// ejecuta si falta). Sin hash aún: se valida solo que exista (TODO P0).
// ---------------------------------------------------------------------------
bool InstalarNet48() {
    std::wstring ruta = ExeDir() + L"\\redist\\ndp48-x86-x64-offline.exe";
    if (!ArchivoExiste(ruta)) {
        MessageBoxW(nullptr,
            L"No se encontró el instalador offline de .NET 4.8 en redist\\. "
            L"Descárgalo desde https://dotnet.microsoft.com/download/dotnet-framework/net48 "
            L"y colócalo en la carpeta redist\\ junto al ejecutable. "
            L"FUSION-HP abrirá el shell de escritorio (Qt), que no necesita .NET.",
            L"FUSION-HP — Falta .NET 4.8",
            MB_OK | MB_ICONWARNING);
        return false;
    }

    int r = MessageBoxW(nullptr,
        L"FUSION-HP necesita .NET Framework 4.8, que no está instalado.\n"
        L"Se procederá a instalarlo desde el paquete offline incluido.\n"
        L"Esto se hace una sola vez. ¿Deseas continuar?",
        L"FUSION-HP — Instalar .NET 4.8",
        MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = ruta.c_str();
    sei.lpParameters = L"/q /norestart";  // silencioso, sin reinicio
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        MessageBoxW(nullptr, L"No se pudo arrancar el instalador de .NET 4.8.",
                    L"FUSION-HP", MB_OK | MB_ICONERROR);
        return false;
    }
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    return Net48Instalado();
}

// ---------------------------------------------------------------------------
// Proceso hijo dentro del Job Object. Devuelve el handle del proceso
// (CloseHandle del caller) o nullptr si no pudo arrancar.
// ---------------------------------------------------------------------------
HANDLE ArrancarEnJob(const std::wstring& ruta, const std::wstring& dir_trabajo,
                     HANDLE hJob, bool oculto, DWORD* rc_error) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = oculto ? SW_HIDE : SW_SHOWNORMAL;
    PROCESS_INFORMATION pi = {};
    std::wstring ruta_mutable = ruta;
    if (!CreateProcessW(ruta_mutable.data(), nullptr, nullptr, nullptr, FALSE,
                        (oculto ? CREATE_NO_WINDOW : 0), nullptr,
                        dir_trabajo.c_str(), &si, &pi)) {
        if (rc_error) *rc_error = GetLastError();
        return nullptr;
    }
    if (hJob) AssignProcessToJobObject(hJob, pi.hProcess);
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Instancia única: si otro launcher (con su job) ya está corriendo,
    // no arrancar un segundo árbol de procesos.
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"FusionHP-Launcher-Running");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
            L"FUSION-HP ya está en ejecución.",
            L"FUSION-HP", MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    RegistrarLinea(L"Arranque de FUSION-HP (launcher)");

    // 1. Verificar .NET 4.8 (la consola gestionada lo necesita)
    bool hay_net48 = Net48Instalado();
    if (!hay_net48) {
        RegistrarLinea(L".NET 4.8 no detectado");
        hay_net48 = InstalarNet48();
        RegistrarLinea(hay_net48 ? L".NET 4.8 quedó instalado"
                                 : L".NET 4.8 sigue ausente (se omitió o falló)");
    } else {
        RegistrarLinea(L".NET 4.8 detectado");
    }

    // 2. Job Object: los hijos mueren con el launcher y no quedan colgados.
    HANDLE hJob = CreateJobObjectW(nullptr, L"FusionHP-Job");
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                &jeli, sizeof(jeli));
    }

    // 3. Núcleo (dueño de la proyección, sin UI propia)
    HANDLE hNucleo = nullptr;
    {
        std::wstring ruta = ExeDir() + L"\\FusionCore.exe";
        DWORD rc = 0;
        if (ArchivoExiste(ruta)) {
            hNucleo = ArrancarEnJob(ruta, ExeDir(), hJob, true, &rc);
        }
        if (!hNucleo) {
            RegistrarLinea(L"FusionCore.exe no arrancó: " +
                           (ArchivoExiste(ruta)
                                ? MensajeError(rc)
                                : L"no se encontró FusionCore.exe"));
        } else {
            // Verificar que SIGUE vivo (antes se registraba "arrancado" y
            // podía morir al segundo sin dejar rastro: el "no abre Core").
            DWORD espera = WaitForSingleObject(hNucleo, 2500);
            if (espera == WAIT_OBJECT_0) {
                DWORD codigo = 0;
                GetExitCodeProcess(hNucleo, &codigo);
                wchar_t m[128];
                swprintf_s(m, 128,
                           L"FusionCore.exe MURIÓ al arrancar (código %u): "
                           L"ver runtime\\nucleo.log",
                           codigo);
                RegistrarLinea(m);
            } else {
                RegistrarLinea(L"FusionCore.exe vivo tras 2,5 s");
            }
            // El pipe IPC del núcleo debe existir (cliente de la consola).
            if (WaitNamedPipeW(L"\\\\.\\pipe\\FusionHP-ipc", 3000)) {
                RegistrarLinea(L"pipe IPC \\\\.\\pipe\\FusionHP-ipc listo");
            } else {
                RegistrarLinea(L"AVISO: el pipe IPC no respondió a tiempo "
                               L"(el núcleo puede seguir arrancando)");
            }
        }
    }

    // 4. Consola gestionada (WinForms). Ruta correcta del paquete:
    //    gestionado\FusionHP.Managed.exe ("managed\" queda como legado).
    HANDLE hGest = nullptr;
    if (hay_net48) {
        std::wstring ruta = ExeDir() + L"\\gestionado\\FusionHP.Managed.exe";
        if (!ArchivoExiste(ruta))
            ruta = ExeDir() + L"\\managed\\FusionHP.Managed.exe";
        DWORD rc = 0;
        if (ArchivoExiste(ruta)) {
            hGest = ArrancarEnJob(ruta, ExeDir(), hJob, false, &rc);
        }
        RegistrarLinea(hGest ? L"Consola gestionada arrancada"
                             : (L"Consola gestionada no arrancó: " +
                                (ArchivoExiste(ruta)
                                     ? MensajeError(rc)
                                     : L"no se encontró FusionHP.Managed.exe")));
    }

    // 5. Alternativa de escritorio: si la consola gestionada no está
    //    disponible (falta .NET o archivos), abrir el shell Qt. Nunca
    //    dejar al usuario sin interfaz si hay alguna instalada.
    HANDLE hQt = nullptr;
    if (!hGest) {
        std::wstring ruta = ExeDir() + L"\\qt\\FusionQtShell.exe";
        DWORD rc = 0;
        if (ArchivoExiste(ruta)) {
            hQt = ArrancarEnJob(ruta, ExeDir() + L"\\qt", hJob, false, &rc);
        }
        RegistrarLinea(hQt ? L"Shell Qt arrancado (alternativa de escritorio)"
                           : (L"Shell Qt no arrancó: " +
                              (ArchivoExiste(ruta)
                                   ? MensajeError(rc)
                                   : L"no se encontró FusionQtShell.exe")));
    }

    if (!hNucleo && !hGest && !hQt) {
        // Nada se abrió: aviso explícito con la ruta, nunca cierre mudo.
        RegistrarLinea(L"ERROR: no se pudo abrir ningún componente");
        MessageBoxW(nullptr,
            L"No se pudo abrir FUSION-HP. Verifica que la carpeta esté "
            L"completa:\r\n"
            L"  FusionCore.exe (núcleo)\r\n"
            L"  gestionado\\FusionHP.Managed.exe (consola, requiere .NET 4.8)\r\n"
            L"  qt\\FusionQtShell.exe (shell de escritorio)\r\n\r\n"
            L"El detalle del arranque quedó en runtime\\arranque.log "
            L"(junto al programa, o en %LOCALAPPDATA%\\FUSION-HP\\runtime "
            L"si está instalado en Archivos de programa).",
            L"FUSION-HP — No se pudo abrir",
            MB_OK | MB_ICONERROR);
    }

    // 6. Esperar a que los hijos terminen. Al cerrar la consola (o el
    //    shell Qt alternativo) se corta también el núcleo: el launcher
    //    cierra el job y con él muere todo proceso restante.
    if (hGest || hQt) {
        const HANDLE esperas[2] = { hGest, hQt };
        const HANDLE* lista = esperas;
        DWORD cuenta = 0;
        for (DWORD i = 0; i < 2; ++i)
            if (lista[i]) ++cuenta;
        HANDLE ordenados[2];
        DWORD n = 0;
        for (DWORD i = 0; i < 2; ++i)
            if (lista[i]) ordenados[n++] = lista[i];
        // Espera mientras haya UI viva. El núcleo la sobrevive si el
        // operador la cierra desde la proyección; al terminar la UI,
        // cortamos el árbol completo (nada colgado).
        if (n > 0) WaitForMultipleObjects(n, ordenados, TRUE, INFINITE);
        RegistrarLinea(L"Interfaz cerrada: cortando el núcleo y saliendo");
    } else if (hNucleo) {
        // Solo núcleo (sin UI disponible, ya avisado): esperar a que muera.
        WaitForSingleObject(hNucleo, INFINITE);
        RegistrarLinea(L"Núcleo terminado: saliendo");
    }

    if (hNucleo) CloseHandle(hNucleo);
    if (hGest) CloseHandle(hGest);
    if (hQt) CloseHandle(hQt);
    if (hJob) CloseHandle(hJob);   // KILL_ON_JOB_CLOSE: no quedan colgados
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
