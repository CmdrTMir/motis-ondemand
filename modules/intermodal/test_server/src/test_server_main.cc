#include <iostream>
#include <thread>
#include "net/stop_handler.h"
#include "test_server_impl.h"

int main(int argc, char** argv) {
  std::cout << "|~~~ Welcome to the ondemand test server! ~~~| \n";
  auto server_arguments = std::vector<std::string>{};
  std::string load = "low";
  std::string area = "0";
  std::string fleet = "off";
  int threads = 32;
  for (int i = 1; i < argc; i = i+2) {
    std::string arg = argv[i];
    if ((arg == "-h") || (arg == "--help")) {
      std::cerr << "Usage of " << argv[0] << "\n"
                << "Options: (default value in [] brackets)\n"
                << "\t-h, --help\tShow this help message.\n"
                << "\t-l, --load\tPossible options: low, medium, high. "
                   "To simulate the compute time for the server [low]\n"
                << "\t-t, --threads\tPossible options: integer value. "
                   "Best option: let it run on the same value as motis [32]\n"
                << "\t-a, --area\tPossible options: 0,1,2,3. "
                   "On which area configuration should the server run? [swiss complete: 0]\n"
                << "\t-f, --fleet\tPossible options: little (20), normal (27), big (35), off. "
                   "Choose how many cars the ondemand service has [off] \n"
                << std::endl;
      return 0;
    } else if ((arg == "-l") || (arg == "--load")) {
      if (i + 1 < argc) {
        server_arguments.emplace_back(argv[i+1]);
        load = argv[i+1];
      } else {
        std::cerr << "--load option requires one argument: "
                     "'low' or 'medium' or 'high' \t"
                  << "Or use --help to show usage information\n"
                  << std::endl;
        return 1;
      }
    } else if((arg == "-a") || (arg == "--area")) {
      if (i + 1 < argc) {
        server_arguments.emplace_back(argv[i+1]);
        area = argv[i+1];
      } else {
        std::cerr << "--area option requires one argument: "
                     "'0' or '1' or '2' or '3' for an area in which the ondemand"
                     "transport is working \t"
                  << "Or use --help to show usage information\n"
                  << std::endl;
        return 1;
      }
    } else if((arg == "-f") || (arg == "--fleet")) {
      if (i + 1 < argc) {
        server_arguments.emplace_back(argv[i+1]);
        fleet = argv[i+1];
      } else {
        std::cerr << "--fleet option requires one argument: "
                     "'little' or 'normal' or 'big' or 'off' for the fleet size\t"
                  << "Or use --help to show usage information\n"
                  << std::endl;
        return 1;
      }
    } else if((arg == "-t") || (arg == "--threads")) {
      if (i + 1 < argc) {
        threads = std::stoi(argv[i+1]);
      } else {
        std::cerr << "--threads option requires one integer argument: "
                     "number of threads. \t"
                  << "Or use --help to show usage information\n"
                  << std::endl;
        return 1;
      }
    } else {
      std::cout << "This is NOT an option. \n"
                   "Use --help to to show usage information and possible server options. \n"
                   "All default values will be used.";
    }
  }

  std::cout << "\n|---- Options in use: ----|\n"
               "|-- load option: " << load << "\n"
               "|-- fleet option: " << fleet << "\n"
               "|-- area option: " << area << "\n"
               "|-- \n"
               "|-- thread count: " << std::to_string(threads) << "\n"
               "|-------------------------|\n\n";

  boost::asio::io_context ioc;
  motis::intermodal::test_server servertest(ioc, server_arguments);
  boost::system::error_code ectest;
  servertest.listen_tome("127.0.0.1", "9000", ectest);
  if (ectest) {
    std::cout << "unable to start testserver: " << ectest.message() << "\n";
    return 1;
  }
  std::vector<std::thread> v;
  v.reserve(threads - 1);
  for(auto i = threads - 1; i > 0; --i) {
    v.emplace_back([&ioc] { ioc.run(); });
  }
  ioc.run();
  return 0;
}

