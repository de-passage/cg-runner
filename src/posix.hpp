#ifndef HEADER_GUARD_DPSG_POSIX_HPP
#define HEADER_GUARD_DPSG_POSIX_HPP

#include <iostream>
#include <span>
#include <chrono>
#include <cstdint>
#include <cassert>
#include <vector>

namespace dpsg {
namespace unix {
extern "C" {
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
}
} // namespace unix

enum class pid_t : uint64_t {};

enum class fd_t : int {};

template <class T, class Enum = T> struct integer_result {
  T _value;

  constexpr static inline T error_bit = (T)((T)1 << (sizeof(T) * 8 - 1));

  constexpr bool is_error() const { return (error_bit & _value) != 0; }

  constexpr bool is_value() const { return (error_bit & _value) == 0; }

  constexpr T value() const {
    assert(is_value() && "integer_result is an error");
    return _value;
  }

  constexpr Enum error() const {
    assert(is_error() && "integer_result is not an error");
    return (Enum)(_value ^ error_bit);
  }

  static inline integer_result from_errno() noexcept {
    return {errno | error_bit};
  }

  static inline integer_result from_unknown(T value) noexcept {
    return (value < 0 ? from_errno() : integer_result{value});
  }

  static inline integer_result from_error(Enum error) noexcept {
    return integer_result{(std::underlying_type_t<Enum>)(error) | error_bit};
  }
};

using int_err = integer_result<int>;
using long_err = integer_result<long>;


inline long_err read(fd_t fd, char *buffer, size_t size) {
  return long_err::from_unknown(unix::read((int)fd, buffer, size));
}

template <size_t S> long_err read(fd_t fd, char (&buffer)[S]) {
  return long_err::from_unknown(unix::read((int)fd, buffer, S));
}

inline long_err write(fd_t fd, const char *buffer, size_t size) {
  return long_err::from_unknown(unix::write((int)fd, buffer, size));
}

template <class F> pid_t fork(F &&f) {

  volatile int p = unix::fork();
  switch (p) {
  case -1:
    perror("Fork failed");
    exit(-1);
  case 0:
    if constexpr (std::is_same_v<std::invoke_result_t<F>, void>) {
      f();
      exit(0);
    } else {
      exit(f());
    }
  default:
    return (pid_t)p;
  }
}

inline pid_t getpid() { return (pid_t)unix::getpid(); }

struct wait_status {
  int error;
  int status;

  constexpr bool success() const { return error == 0; }

  constexpr bool terminated() const {
    return success() && (WIFEXITED(status) || WIFSIGNALED(status));
  }

  inline bool exited() const { return success() && WIFEXITED(status); }

  constexpr inline bool signaled() const {
    return success() && WIFSIGNALED(status);
  }

  constexpr inline int term_signal() const { return WTERMSIG(status); }

  constexpr inline int exit_status() const { return WEXITSTATUS(status); }
};

struct process_t {
  pid_t pid;
  fd_t stdout;
  fd_t stdin;
  fd_t stderr;

  wait_status wait(int options = WUNTRACED | WCONTINUED) {
    int status;
    unix::waitpid((int)pid, &status, options);
    return wait_status{.error = errno, .status = status};
  }
};

inline process_t run_external(std::string_view name, const char *const *args) {
  enum RW { Read = 0, Write = 1 };
  int in[2], err[2], out[2];
  auto e = unix::pipe(in);
  if (e == -1) {
    perror("Pipe opening failed");
    exit(1);
  }
  if ((e = unix::pipe(out)) == -1) {
    perror("Pipe opening failed");
  }
  if ((e = unix::pipe(err)) == -1) {
    perror("Pipe opening failed");
  }

  auto p = fork([&]() {
    // We don't need to write on stdin or read from stdout/stderr
    // ignoring errors as there's nothing to do about them
    unix::close(in[Write]);
    unix::close(out[Read]);
    unix::close(err[Read]);
    if (unix::dup2(in[Read], STDIN_FILENO) == -1) {
      perror("Failed to rebind stdin");
      exit(1);
    }
    if (unix::dup2(out[Write], STDOUT_FILENO) == -1) {
      perror("Failed to rebind stdin");
      exit(1);
    }
    if (unix::dup2(in[Read], STDIN_FILENO) == -1) {
      perror("Failed to rebind stdin");
      exit(1);
    }
    unix::close(in[Read]);
    unix::close(out[Write]);
    unix::close(err[Write]);
    unix::execvp(name.data(), (char **)args);
  });

  unix::close(err[Write]);
  unix::close(in[Read]);
  unix::close(out[Write]);

  process_t pr{
      .pid = p,
      .stdout = (fd_t)out[Read],
      .stdin = (fd_t)in[Write],
      .stderr = (fd_t)err[Read],
  };

  return pr;
}

template <size_t BufferSize = 4096>
struct fd_streambuf : std::basic_streambuf<char> {
protected:
  fd_t _file_descriptor;
  constexpr static inline auto _buffer_size = BufferSize;
  char _buffer[_buffer_size];

  virtual int underflow() override {
    if (this->gptr() == this->egptr()) {
      auto read_count = read(_file_descriptor, _buffer);
      if (read_count.is_error() || read_count.value() == 0) {
        return traits_type::eof();
      }
      this->setg(_buffer, _buffer, _buffer + read_count.value());
    }
    return traits_type::to_int_type(*this->gptr());
  }

  virtual int overflow(int c = traits_type::eof()) override {
    if (c != traits_type::eof()) {
      *this->pptr() = traits_type::to_char_type(c);
      this->pbump(1);
    }
    if (sync() == -1)
      return traits_type::eof();
    return c;
  }

  virtual int sync() override {
    auto written = write(_file_descriptor, pbase(), pptr() - pbase());
    if (written.is_error() || written.value() == 0) {
      this->setp(_buffer, _buffer + _buffer_size + 1);
    }
    return written.value();
  }

public:
  fd_streambuf(fd_t file_descriptor) : _file_descriptor(file_descriptor) {
    this->setg(_buffer, _buffer, _buffer);
    this->setp(_buffer, _buffer + _buffer_size - 1);
  }
};

struct process_streams : private process_t {
  inline process_streams(process_t p) noexcept : process_t{p} {}

  fd_streambuf<> stdout_buf{process_t::stdout};
  fd_streambuf<> stdin_buf{process_t::stdin};
  fd_streambuf<> stderr_buf{process_t::stderr};
  std::istream stdout{&stdout_buf};
  std::istream stderr{&stderr_buf};
  std::ostream stdin{&stdin_buf};
};

enum class poll_event_t : short {
  write_ready = POLLOUT,
  error_condition = POLLERR,
  hangup = POLLHUP,
  invalid = POLLNVAL,
  read_ready = POLLIN,
  exception = POLLPRI,
};

constexpr poll_event_t operator|(poll_event_t left,
                                 poll_event_t right) noexcept {
  return (poll_event_t)((short)left | (short)right);
}

constexpr poll_event_t operator&(poll_event_t left,
                                 poll_event_t right) noexcept {
  return (poll_event_t)((short)left & (short)right);
}

struct pollfd : unix::pollfd {
  pollfd(fd_t file_descriptor, poll_event_t event = (poll_event_t)0)
      : unix::pollfd{(int)file_descriptor, (short)event, 0} {}

  void invalidate() { this->fd = ~this->fd; }
};

template <class R = int64_t, class P = std::milli>
integer_result<int>
poll(std::span<pollfd> pollfds,
     std::chrono::duration<R, P> timeout = std::chrono::milliseconds(0)) {
  auto timeout_i =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
  return integer_result<int>::from_unknown(
      unix::poll(pollfds.data(), pollfds.size(), timeout_i.count()));
}

template <class F, class R = int64_t, class P = std::milli>
integer_result<int>
poll(std::span<fd_t> fds, poll_event_t event, F &&func,
     std::chrono::duration<R, P> timeout = std::chrono::milliseconds(0)) {
  std::vector<pollfd> pollfds;
  pollfds.reserve(fds.size());
  for (auto fd : fds) {
    pollfds.emplace_back(fd, event);
  }

  auto r = poll(pollfds, timeout);
  if (r.is_value()) {
    for (size_t s = 0; s < pollfds.size(); ++s) {
      auto &p = pollfds[s];
      if (p.revents == 0 || p.fd < 0)
        continue;
      if constexpr (std::is_invocable_v<F, pollfd, size_t>) {
        func(p, s);
      } else {
        func(p);
      }
    }
  }
  return r;
}

template <
    class T, class G, class F, class R = int64_t, class P = std::milli,
    std::enable_if_t<std::is_same_v<std::invoke_result_t<std::decay_t<G>, std::add_lvalue_reference_t<std::decay_t<T>>>, fd_t>, int> = 0>
integer_result<int>
poll(std::span<T> fds, G &&getter, poll_event_t event, F &&func,
     std::chrono::duration<R, P> timeout = std::chrono::milliseconds(0)) {
  std::vector<pollfd> pollfds;
  pollfds.reserve(fds.size());
  for (auto fd : fds) {
    pollfds.emplace_back(getter(fd), event);
  }

  auto r = poll(pollfds, timeout);
  if (r.is_value()) {
    std::cerr << "Poll returned " << r.value() << std::endl;
    for (size_t s = 0; s < pollfds.size(); ++s) {
      auto &p = pollfds[s];
      if (p.revents == 0)
        continue;
      if constexpr (std::is_invocable_v<F, pollfd&, size_t>) {
        func(p, s);
      } else {
        func(p);
      }
    }
  }
  return r;
}

template <class F, class R = int64_t, class P = std::milli>
void poll_one_each(const std::span<process_t> &ps, poll_event_t ev, F &&func) {
  auto s = ps.size();
  std::cerr << "Expecting " << s << " results" << std::endl;
  size_t n = 0;
  while (n < s) {
    poll(
        ps, [](process_t &p) { return p.stdout; }, ev,
        [&](pollfd &p, size_t idx) {
          func(ps[idx]);
          std::cerr << "Invalidating" << (int)p.fd << std::endl;
          p.invalidate();
          n++;
        });
  }
}

struct never {
  never() = delete;
  never(const never &) = delete;
  never(never &&) = delete;
  ~never() = delete;
  never &operator=(const never &) = delete;
  never &operator=(never &&) = delete;
};

} // namespace dpsg

#endif // HEADER_GUARD_DPSG_POSIX_HPP
