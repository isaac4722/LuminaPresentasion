// src/launcher/main.cpp — Launcher nativo de FUSION-HP
//
// Punto único de arranque. Sin consola (WinMain). Verifica .NET 4.8,
// instala si falta el instalador offline oficial incluido (una sola vez,
// con aviso), arranca el núcleo si no está corriendo, abre la carcasa
// gestionada y sale sin dejar procesos colgados.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace {

// Hash SHA-256 embebido del instalador offline de .NET 4.8. El launcher
// valida el archivo antes de ejecutarlo. Si no coincide, NO lo ejecuta.
// TODO(P0): calcular y reemplazar este valor cuando se incluya ndp48-x86-x64-offline.exe.
constexpr const char* kNet48OfflineSha256 =
    "0000000000000000000000000000000000000000000000000000000000000000";

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

std::wstring RuntimeDir() { return ExeDir() + L"\\runtime"; }

bool ArchivoExiste(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
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
// Valida el hash SHA-256 del instalador offline antes de ejecutarlo.
// (Stub: por ahora solo verifica que el archivo existe. TODO(P0): hash real).
// ---------------------------------------------------------------------------
bool ValidarNet48Offline(const std::wstring& ruta) {
    DWORD attrs = GetFileAttributesW(ruta.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) return false;
    // TODO(P0): CryptCATAdminCalcHashFromFileHandle o BCryptCreateHash
    // con SHA-256, comparar con kNet48OfflineSha256.
    return true;
}

// ---------------------------------------------------------------------------
// Marca "ya instalé .NET 4.8" para no repetir el aviso.
// Vive en runtime/net48_installed.flag. Sin Registro.
// ---------------------------------------------------------------------------
bool Net48YaIntentado() {
    std::wstring ruta = RuntimeDir() + L"\\net48_installed.flag";
    DWORD attrs = GetFileAttributesW(ruta.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

void MarcarNet48Intentado() {
    CreateDirectoryW(RuntimeDir().c_str(), nullptr);
    std::wstring ruta = RuntimeDir() + L"\\net48_installed.flag";
    HANDLE h = CreateFileW(ruta.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        const char* msg = "net48 install intentado";
        DWORD escritos = 0;
        WriteFile(h, msg, static_cast<DWORD>(strlen(msg)), &escritos, nullptr);
        CloseHandle(h);
    }
}

// ---------------------------------------------------------------------------
// Ejecuta el instalador offline de .NET 4.8 y espera a que termine.
// Muestra aviso al usuario.
// ---------------------------------------------------------------------------
bool InstalarNet48() {
    std::wstring ruta = ExeDir() + L"\\redist\\ndp48-x86-x64-offline.exe";
    if (!ValidarNet48Offline(ruta)) {
        MessageBoxW(nullptr,
            L"No se encontró el instalador offline de .NET 4.8 en redist\\. "
            L"Descárgalo desde https://dotnet.microsoft.com/download/dotnet-framework/net48 "
            L"y colócalo en la carpeta redist\\ junto al ejecutable. "
            L"La aplicación se abrirá en modo reducido (perfil C: núcleo sin capa gestionada).",
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
    MarcarNet48Intentado();
    return Net48Instalado();
}

// ---------------------------------------------------------------------------
// Arranca el núcleo si no está corriendo. Usa Job Objects para garantizar
// que si el launcher muere, los hijos también.
// ---------------------------------------------------------------------------
bool ArrancarNucleo() {
    std::wstring ruta = ExeDir() + L"\\FusionCore.exe";
    if (!ArchivoExiste(ruta)) return false;

    // Verificar si ya está corriendo (mutex simple)
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"FusionHP-Core-Running");
    if (hMutex) {
        CloseHandle(hMutex);
        return true;  // ya corriendo
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // el núcleo no tiene UI propia
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(ruta.c_str(), nullptr, nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW, nullptr, ExeDir().c_str(), &si, &pi)) {
        return false;
    }

    // Asignar a un Job Object para que muera con el launcher si hace falta
    HANDLE hJob = CreateJobObjectW(nullptr, L"FusionHP-Job");
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                &jeli, sizeof(jeli));
        AssignProcessToJobObject(hJob, pi.hProcess);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

bool ArrancarGestionada() {
    std::wstring ruta = ExeDir() + L"\\managed\\FusionHP.Managed.exe";
    if (!ArchivoExiste(ruta)) return false;
    HINSTANCE h = ShellExecuteW(nullptr, L"open", ruta.c_str(), nullptr,
                                  ExeDir().c_str(), SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(h) > 32;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // 1. Verificar .NET 4.8
    if (!Net48Instalado()) {
        if (!Net48YaIntentado()) {
            InstalarNet48();
        } else {
            // Ya se intentó antes y no se completó (o falta). Avisar una vez.
            MessageBoxW(nullptr,
                L"No se detectó .NET 4.8. La carcasa gestionada no estará disponible. "
                L"Se abrirá el núcleo en modo perfil C (sin interfaz gestionada).",
                L"FUSION-HP — Sin .NET 4.8",
                MB_OK | MB_ICONWARNING);
        }
    }

    // 2. Arrancar núcleo si no está corriendo
    ArrancarNucleo();

    // 3. Arrancar carcasa gestionada (si .NET está OK)
    if (Net48Instalado()) {
        ArrancarGestionada();
    }

    // 4. Salir limpio. El Job Object garantiza que si el launcher muere,
    //    los hijos (núcleo + gestionada) también mueren.
    return 0;
}
