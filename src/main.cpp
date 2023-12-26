#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string_view>
#include <type_traits>
#include <thread>
#include <chrono>
#include <cassert>

#include "vt100.hpp"

namespace dpsg {
enum class pid_t : uint64_t {};

template <class T>
struct std::basic_ostream<T> &operator<<(struct std::basic_ostream<T> &out,
                                         pid_t p) {
  return out << (int)p;
}

enum class fd_t : int {};

template<class T, class Enum = T>
  struct integer_result {
    T _value;

    constexpr static inline T error_bit = (T)(1 << (sizeof(T) - 1));

    constexpr bool is_error() const {
      return (error_bit & _value) != 0;
    }

    constexpr bool is_value() const {
      return (error_bit & _value) == 0;
    }

    constexpr T value() const {
      assert(is_value() && "integer_result is an error");
      return _value;
    }

    constexpr Enum error() const {
      assert(is_error() && "integer_result is not an error");
      return (Enum)(_value ^ error_bit);
    }

    static inline integer_result from_errno() noexcept {
      return {errno & error_bit};
    }

    static inline integer_result from_unknown(T value) noexcept {
      return (value < 0 ? from_errno() : integer_result{value});
    }
  };

using int_err = integer_result<int>;
using long_err = integer_result<long>;

namespace unix {
  extern "C" {
#include <unistd.h>
#include <sys/wait.h>
  }
}

long_err read(fd_t fd, char* buffer, size_t size) {
  return long_err::from_unknown(unix::read((int)fd, buffer, size));
}

template <size_t S>
long_err read(fd_t fd, char (&buffer)[S]) {
  return long_err::from_unknown(unix::read((int)fd, buffer, S));
}

long_err write(fd_t fd, const char* buffer, size_t size) {
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

pid_t getpid() { return (pid_t)unix::getpid(); }

struct wait_status {
  int error;
  int status;

  constexpr bool success() const {
    return error == 0;
  }

  constexpr bool terminated() const {
    return success() && (WIFEXITED(status) || WIFSIGNALED(status));
  }

  inline bool exited() const {
    return success() && WIFEXITED(status);
  }

  constexpr inline bool signaled() const {
    return success() && WIFSIGNALED(status);
  }

  constexpr inline int term_signal() const {
    return WTERMSIG(status);
  }

  constexpr inline int exit_status() const {
    return WEXITSTATUS(status);
  }

};

struct process_t {
  pid_t pid;
  fd_t stdout;
  fd_t stdin;
  fd_t stderr;

  wait_status wait(int options = WUNTRACED | WCONTINUED) {
    int status;
    unix::waitpid((int)pid, &status, options);
    perror("waitpid code");
    return wait_status{.error = errno, .status = status};
  }
};

process_t run_external(std::string_view name, const char * const *args) {
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
    unix::execvp(name.data(), (char**)args);
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

template<size_t BufferSize = 4096>
struct fd_read_streambuf : std::basic_streambuf<char> {
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
    if (sync() == -1) return traits_type::eof();
    return c;
  }

  virtual int sync() override {
    auto written = write(_file_descriptor, pbase(), pptr() - pbase());
    if (written.is_error() || written.value() == 0) {
      this->setp(_buffer, _buffer+_buffer_size + 1);
    }
    return written.value();
  }

  public:
  fd_read_streambuf(fd_t file_descriptor) : _file_descriptor(file_descriptor) {
    this->setg(_buffer, _buffer, _buffer);
    this->setp(_buffer, _buffer + _buffer_size - 1);
  }
};

struct process_streams : private process_t {
  inline process_streams(process_t p) noexcept : process_t{p} {}

  fd_read_streambuf<> stdout_buf{process_t::stdout};
  fd_read_streambuf<> stdin_buf{process_t::stdin};
  fd_read_streambuf<> stderr_buf{process_t::stderr};
  std::istream stdout{&stdout_buf};
  std::istream stderr{&stderr_buf};
  std::ostream stdin{&stdin_buf};
};

} // namespace dpsg

int print_my_pid() {
  std::cout << "My pid is " << dpsg::getpid() << std::endl;
  return 0;
}

auto run() {
  const char* params[] = {
    "java", "-jar", "/home/depassage/workspace/codingame-fall2023/ais/referee.jar", "-p1", "/home/depassage/workspace/codingame-fall2023/ais/basic", "-p2", "/home/depassage/workspace/codingame-fall2023/ais/basic-hunter", "-l", "output.json" };
  return dpsg::run_external(params[0], params);
}

int main() {
  auto pr = run();
  dpsg::wait_status status;
  using namespace std::literals;
  std::cout << dpsg::vt100::bold << "Launched with pid " << (dpsg::vt100::reverse) << pr.pid << dpsg::vt100::reset << std::endl;
  std::string out_str, err_str;

  dpsg::process_streams streams{pr};
  int color = 0;
  do {
    char buffer[128];
    dpsg::long_err n{};
    // while ((n = dpsg::read(pr.stdout, buffer)).value() > 0) {
    //   out_str += std::string_view(buffer, n.value());
    // }
    while (std::getline(streams.stdout, out_str)) {
      color = (color+1) % 6;
      std::cout << dpsg::vt100::fg((dpsg::vt100::color)(color + 1)) << out_str << dpsg::vt100::reset << std::endl;
    }
    while ((n = dpsg::read(pr.stderr, buffer)).value() > 0) {
      err_str += std::string_view(buffer, n.value());
    }
    status = pr.wait();
  } while (!status.terminated());

  if (status.exited()) {
    std::cout << "Exited with status: " << (dpsg::vt100::bold|dpsg::vt100::reverse|dpsg::vt100::underline) << status.exit_status() << dpsg::vt100::reset << std::endl;
  }
  if (!out_str.empty()) {
    std::cout << dpsg::vt100::blue << out_str << dpsg::vt100::reset << std::endl;
  }
  if (!err_str.empty()) {
    std::cerr << dpsg::vt100::red << err_str << dpsg::vt100::reset << std::endl;
  }

  return 0;
}
