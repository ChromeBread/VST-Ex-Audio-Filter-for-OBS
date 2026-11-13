#pragma once

// Optional log file next to the settings (Tools > VST Ex Audio Filter > Debug mode > Log). Off by default.
#ifdef __cplusplus
#include <QString>

QString log_file_path();
bool log_file_enabled();
void log_file_set_enabled(bool on);

extern "C" {
#endif

void log_file_write(int log_level, const char *text);

#ifdef __cplusplus
}
#endif
