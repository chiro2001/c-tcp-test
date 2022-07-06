#include <unistd.h>

#include "macro.h"

#define LOG_SELF MUXDEF(SELF, SELF, "srv")

#define LOG_(fp, name, format, ...)                                   \
  do {                                                                \
    printf("[%s](%d) " format, name, (int)(getpid()), ##__VA_ARGS__); \
    if (fp) {                                                         \
      fprintf(fp, format, ##__VA_ARGS__);                             \
      fflush(fp);                                                     \
    }                                                                 \
  } while (0);

#define LOG(fp, format, ...) LOG_(fp, LOG_SELF, format, ##__VA_ARGS__)

#define LOG_RQT(fp, format, ...) LOG_(fp, format, ##__VA_ARGS__)

#define LG(format, ...) LOG(NULL, format, ##__VA_ARGS__)

void setup_signal_handler(int signal_number, void (*function)(int)) {
  struct sigaction sig = {.sa_flags = SA_RESTART,
                          .__sigaction_handler = function};
  sigemptyset(&sig.sa_mask);
  sigaction(signal_number, &sig, NULL);
}
