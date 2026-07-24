#pragma once
#include <string>

// ─────────────────────────────── LOGGER ──────────────────────────────────
// Ghi đồng thời ra console (có màu) và file dumper.log (plain text).
// Dùng: Log::Info("..."), Log::Warn("..."), Log::Err("..."),
//        Log::Progress(done, total, "label")
class Log
{
public:
    // Khởi tạo: in banner và flush log header
    static void Init(const std::string& title);

    static void Info(const std::string& msg);
    static void Warn(const std::string& msg);
    static void Err(const std::string& msg);
    static void Step(const std::string& msg);

    // Progress bar: [=====>    ] 53% (done/total label)
    static void Progress(size_t done, size_t total, const std::string& label);

    static void ProgressDone(const std::string& label);

    // Thụt lề – dùng để in detail dưới một Info/Step
    static void Detail(const std::string& msg);
};
