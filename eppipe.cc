/*
 * Copyright (c) 2012 Yelp, Inc.
 *
 * Original version written by James Brown (roguelazer@gmail.com), ported to C++
 * and boost::asio by Evan Klitkze (evan@eklitzke.org).
 *
 * Available under the ISC License. For details, see LICENSE
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <functional>


enum { READ_SIZE = 8192 };

struct SignalReceivedExc : std::exception {};

// Handler for SIGCHLD
void SigHandler(int signal __attribute__((unused))) {
  throw SignalReceivedExc();
}

// Handler for read events on stdin -- doesn't actually do anything with the
// data, instead the handler just reschedules itself when there wasn't a read
// error.
void StdinHandler(boost::asio::posix::stream_descriptor *desc,
                  std::vector<char> *buffer,
                  const boost::system::error_code &ec,
                  std::size_t bytes_read __attribute__((unused))) {
  if (!ec) {
    desc->async_read_some(boost::asio::buffer(*buffer),
                          std::bind(StdinHandler, desc, buffer,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
  }
}

// Ensures that a pid is killed when the manager goes out of scope
class PidManager {
 public:
  explicit PidManager(pid_t pid) :pid_(pid) {}
  ~PidManager() { kill(pid_, SIGTERM); }
 private:
  pid_t pid_;
};

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: eppipe command\n"
            "Runs `command` under eppipe\n");
    return 2;
  }

  // Set up a signalfd to handle the SIGCHLD
  sigset_t sigset;
  if (sigemptyset(&sigset)) {
    perror("sigemptyset");
    return EXIT_FAILURE;
  }
  if (sigaddset(&sigset, SIGCHLD)) {
    perror("sigaddset");
    return EXIT_FAILURE;
  }
  signal(SIGCHLD, SigHandler);

  pid_t pid = fork();
  if (pid == 0) {
    if (execvp(argv[1], argv + 1)) {
      fprintf(stderr, "Tried to exec '%s'\n", argv[1]);
      perror("execlp");
      return EXIT_FAILURE;
    }
  } else if (pid < 0) {
    perror("fork");
    return EXIT_FAILURE;
  }

  std::vector<char> buffer(READ_SIZE);
  PidManager manager(pid);
  boost::asio::io_service ios;
  boost::asio::posix::stream_descriptor desc(ios, STDIN_FILENO);

  desc.async_read_some(boost::asio::buffer(buffer),
                       std::bind(StdinHandler, &desc, &buffer,
                                 std::placeholders::_1,
                                 std::placeholders::_2));

  try {
    ios.run();
  } catch (const SignalReceivedExc &exc) { }
  return EXIT_SUCCESS;
}
