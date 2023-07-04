#ifndef ART_PERFETTO_HPROF_PARSE_SMAPS_H_
#define ART_PERFETTO_HPROF_PARSE_SMAPS_H_

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cinttypes>
#include <string>

namespace perfetto_hprof {

struct SmapsEntry {
  int64_t size_kb = -1;
  int64_t private_dirty_kb = -1;
  int64_t swap_kb = -1;
  std::string pathname;
};

struct SmapsParserState {
  bool parsed_header = false;
  SmapsEntry current_entry{};
};

template <typename T>
static bool ParseSmaps(FILE* f, T callback) {
  SmapsParserState state;

  size_t line_size = 1024;
  char* line = static_cast<char*>(malloc(line_size));

  for (;;) {
    errno = 0;
    ssize_t rd = getline(&line, &line_size, f);
    if (rd == -1) {
      free(line);
      if (state.parsed_header)
        callback(state.current_entry);
      return errno == 0;
    } else {
      if (line[rd - 1] == '\n') {
        line[rd - 1] = '\0';
        rd--;
      }
      if (!ParseSmapsLine(line, static_cast<size_t>(rd), &state, callback)) {
        free(line);
        return false;
      }
    }
  }
}

static inline const char* FindNthToken(const char* line,
                                       size_t n,
                                       size_t size) {
  size_t tokens = 0;
  bool parsing_token = false;
  for (size_t i = 0; i < size; ++i) {
    if (!parsing_token && line[i] != ' ') {
      parsing_token = true;
      if (tokens++ == n)
        return line + static_cast<ssize_t>(i);
    }
    if (line[i] == ' ')
      parsing_token = false;
  }
  return nullptr;
}

template <typename T>
static bool ParseSmapsLine(char* line,
                           size_t size,
                           SmapsParserState* state,
                           T callback) {
  char* first_token_end = static_cast<char*>(memchr(line, ' ', size));
  if (first_token_end == nullptr || first_token_end == line)
    return false;  // Malformed.
  bool is_header = *(first_token_end - 1) != ':';

  if (is_header) {
    if (state->parsed_header)
      callback(state->current_entry);

    state->current_entry = {};
    const char* last_token_begin = FindNthToken(line, 5u, size);
    if (last_token_begin)
      state->current_entry.pathname.assign(last_token_begin);
    state->parsed_header = true;
    return true;
  }
  if (!state->parsed_header)
    return false;

  sscanf(line, "Size: %" PRId64 " kB", &state->current_entry.size_kb);
  sscanf(line, "Swap: %" PRId64 " kB", &state->current_entry.swap_kb);
  sscanf(line, "Private_Dirty: %" PRId64 " kB",
         &state->current_entry.private_dirty_kb);
  return true;
}

}  // namespace perfetto_hprof

#endif  // ART_PERFETTO_HPROF_PARSE_SMAPS_H_
