#pragma once

#include <string>
#include <filesystem>
#include <cstdlib>
#include <iostream>

namespace intelidata::common {

namespace fs = std::filesystem;

/**
 * @brief Utilidad universal para resolución de rutas relativas y variables de entorno.
 * Permite que ejecutables y pruebas localicen archivos de datos (samples/), recursos
 * y directorios de salida con independencia de si se ejecutan desde Visual Studio,
 * Ninja, CMake o línea de comandos.
 */
class PathUtils {
public:
    [[nodiscard]] static fs::path find_project_root() {
        if (const char* env_root = std::getenv("INTELIDATA_ROOT")) {
            if (fs::exists(env_root)) return fs::path(env_root);
        }
        if (const char* ws_root = std::getenv("WORKSPACE_ROOT")) {
            if (fs::exists(ws_root)) return fs::path(ws_root);
        }
        
        fs::path cur = fs::current_path();
        for (int i = 0; i < 6; ++i) {
            if (fs::exists(cur / "CMakeLists.txt") && (fs::exists(cur / "samples") || fs::exists(cur / "include"))) {
                return cur;
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) break;
            cur = cur.parent_path();
        }
        return fs::current_path();
    }

    [[nodiscard]] static fs::path resolve_path(const std::string& raw_path) {
        if (raw_path.empty()) return {};
        fs::path p(raw_path);
        if (fs::exists(p)) return p;

        fs::path root = find_project_root();
        if (fs::exists(root / p)) return root / p;

        if (fs::exists(root / "samples" / p.filename())) {
            return root / "samples" / p.filename();
        }

        return p;
    }

    static void ensure_report_images(const fs::path& target_markdown_file) {
        fs::path target_dir = target_markdown_file.has_parent_path() 
            ? target_markdown_file.parent_path() 
            : fs::current_path();
        fs::path dest_images_dir = target_dir / "images";

        try {
            fs::create_directories(dest_images_dir);
            fs::path root = find_project_root();
            fs::path src_images = root / "docs" / "images";
            if (fs::exists(src_images)) {
                for (const auto& entry : fs::directory_iterator(src_images)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".png") {
                        fs::copy_file(entry.path(), dest_images_dir / entry.path().filename(), 
                                     fs::copy_options::overwrite_existing);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PathUtils] Aviso: No se pudieron copiar graficas: " << e.what() << "\n";
        }
    }
};

} // namespace intelidata::common
