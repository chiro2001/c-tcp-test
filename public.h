#include <unistd.h>

#include "macro.h"

#define LOG_SELF MUXDEF(SELF_CLI, "cli", "srv")
#define LOG_PREFIX "[%s](%d) "

#define LOG_(fp, name, format, ...)                                          \
  do {                                                                       \
    printf(LOG_PREFIX format "\n", name, (int)(getpid()), ##__VA_ARGS__);         \
    if (fp) {                                                                \
      fprintf(fp, LOG_PREFIX format "\n", name, (int)(getpid()), ##__VA_ARGS__); \
      fflush(fp);                                                            \
    }                                                                        \
  } while (0);

#define LOG(fp, format, ...) LOG_(fp, LOG_SELF, format, ##__VA_ARGS__)

#define LOG_RQT(fp, format, ...) LOG_(fp, "echo_rqt", format, ##__VA_ARGS__)
#define LOG_REP(fp, format, ...) LOG_(fp, "echo_rep", format, ##__VA_ARGS__)

#define LG(format, ...) LOG(NULL, format, ##__VA_ARGS__)

void setup_signal_handler(int signal_number, void (*function)(int), int flags) {
  struct sigaction sig = {.sa_flags = flags,
                          .__sigaction_handler = function};
  sigemptyset(&sig.sa_mask);
  sigaction(signal_number, &sig, NULL);
}
